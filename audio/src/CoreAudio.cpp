#include "ragtime/audio/CoreAudioVoice.h"
#include "ragtime/audio/VoiceAudio.h"
#include "ragtime/audio/EarAudio.h"

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <dispatch/dispatch.h>
#include <libproc.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <utility>

namespace ragtime::audio
{
namespace
{
constexpr AudioObjectID system = kAudioObjectSystemObject;
constexpr auto global = kAudioObjectPropertyScopeGlobal;
constexpr auto inputScope = kAudioDevicePropertyScopeInput;
constexpr auto outputScope = kAudioDevicePropertyScopeOutput;

AudioObjectPropertyAddress address(AudioObjectPropertySelector selector, AudioObjectPropertyScope scope)
{
    return {selector, scope, kAudioObjectPropertyElementMain};
}

template<class T>
T property(AudioObjectID object, AudioObjectPropertySelector selector, AudioObjectPropertyScope scope = global)
{
    auto key = address(selector, scope);
    T value{};
    UInt32 size = sizeof(value);
    if (AudioObjectGetPropertyData(object, &key, 0, nullptr, &size, &value) != noErr) return {};
    return value;
}

std::vector<AudioObjectID> objectList(AudioObjectID object, AudioObjectPropertySelector selector,
                                      AudioObjectPropertyScope scope = global)
{
    auto key = address(selector, scope);
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(object, &key, 0, nullptr, &size) != noErr || !size) return {};
    std::vector<AudioObjectID> ids(size / sizeof(AudioObjectID));
    if (AudioObjectGetPropertyData(object, &key, 0, nullptr, &size, ids.data()) != noErr) return {};
    ids.resize(size / sizeof(AudioObjectID));
    return ids;
}

std::string textProperty(AudioObjectID object, AudioObjectPropertySelector selector)
{
    CFStringRef text = property<CFStringRef>(object, selector);
    if (!text) return {};
    auto capacity = CFStringGetMaximumSizeForEncoding(CFStringGetLength(text), kCFStringEncodingUTF8) + 1;
    std::string result(static_cast<size_t>(capacity), '\0');
    if (CFStringGetCString(text, result.data(), capacity, kCFStringEncodingUTF8))
        result.resize(std::strlen(result.c_str()));
    else
        result.clear();
    CFRelease(text);
    return result;
}

AudioDeviceID defaultDevice(bool input)
{
    return property<AudioDeviceID>(system, input ? kAudioHardwarePropertyDefaultInputDevice
                                                : kAudioHardwarePropertyDefaultOutputDevice);
}

bool hasStreams(AudioDeviceID device, bool input)
{
    return !objectList(device, kAudioDevicePropertyStreams, input ? inputScope : outputScope).empty();
}

struct RadioDevices
{
    std::vector<std::string> input;
    std::vector<std::string> output;
};

std::mutex radioMutex;
RadioDevices reserved;

std::vector<std::string> radioUids(const std::string& name, bool input)
{
    std::vector<std::string> result;
    for (auto device : objectList(system, kAudioHardwarePropertyDevices))
    {
        if (!hasStreams(device, input)) continue;
        if (name.empty() ? device != defaultDevice(input) : textProperty(device, kAudioObjectPropertyName) != name)
            continue;
        auto uid = textProperty(device, kAudioDevicePropertyDeviceUID);
        if (!uid.empty()) result.push_back(std::move(uid));
    }
    return result;
}

bool usesDevice(const std::vector<std::string>& uids, const std::string& uid)
{
    return std::find(uids.begin(), uids.end(), uid) != uids.end();
}

struct HostInput
{
    int pid;
    std::string program;
    std::string uid;
};

void addExternalRadioDevices(RadioDevices& radio, std::vector<HostInput>& inputs)
{
    if (__builtin_available(macOS 14.2, *))
    {
        for (auto process : objectList(system, kAudioHardwarePropertyProcessObjectList))
        {
            auto pid = property<pid_t>(process, kAudioProcessPropertyPID);
            char name[PROC_PIDPATHINFO_MAXSIZE]{};
            if (pid <= 0 || proc_name(pid, name, sizeof(name)) <= 0) continue;
            if (!ear::isData2GProgram(name) && std::strcmp(name, "data2g-web") != 0
                && textProperty(process, kAudioProcessPropertyBundleID) != "org.cleverdomain.data2g")
                continue;
            for (bool input : {true, false})
            {
                auto& uids = input ? radio.input : radio.output;
                for (auto device : objectList(process, kAudioProcessPropertyDevices, input ? inputScope : outputScope))
                {
                    auto uid = textProperty(device, kAudioDevicePropertyDeviceUID);
                    if (uid.empty()) continue;
                    if (input) inputs.push_back({pid, ear::isData2GProgram(name) ? name : "data2g-host", uid});
                    uids.push_back(std::move(uid));
                }
            }
        }
    }
}

struct DeviceGraph
{
    ear::Graph graph;
    std::string defaultInput;
    std::string defaultOutput;
    std::vector<std::string> keys;
};

DeviceGraph deviceGraph(bool forVoice)
{
    RadioDevices radio;
    if (forVoice)
    {
        std::lock_guard<std::mutex> lock(radioMutex);
        radio = reserved;
    }
    std::vector<HostInput> inputs;
    addExternalRadioDevices(radio, inputs);
    DeviceGraph result;
    auto& nodes = result.graph.nodes;
    const auto inputDefault = defaultDevice(true), outputDefault = defaultDevice(false);
    for (auto device : objectList(system, kAudioHardwarePropertyDevices))
    {
        auto uid = textProperty(device, kAudioDevicePropertyDeviceUID);
        if (uid.empty()) continue;
        auto label = textProperty(device, kAudioObjectPropertyName);
        for (bool input : {true, false})
        {
            if (!hasStreams(device, input)) continue;
            const bool inUse = usesDevice(input ? radio.input : radio.output, uid);
            const bool isDefault = device == (input ? inputDefault : outputDefault);
            // Use one ID for each graph node and one shared UID for the
            // physical device. Core Audio's device ID can change on reconnect.
            const auto name = uid + (input ? ":input" : ":output");
            nodes.push_back({static_cast<uint32_t>(nodes.size() + 1), 0, name, label,
                             input ? "Audio/Source" : "Audio/Sink", {}, {}, uid});
            result.keys.push_back(name + (inUse ? " radio " : " ") + label + (isDefault ? " default" : ""));
            if (isDefault) (input ? result.defaultInput : result.defaultOutput) = name;
        }
    }
    if (!forVoice)
    {
        for (const auto& input : inputs)
        {
            auto source = std::find_if(nodes.begin(), nodes.end(), [&](const auto& n) {
                return ear::isSource(n) && n.device == input.uid;
            });
            if (source == nodes.end()) continue;
            const auto stream = static_cast<uint32_t>(nodes.size() + 1);
            result.graph.clients.push_back({stream, input.program, input.pid});
            result.graph.links.push_back({source->id, stream});
            nodes.push_back({stream, stream, {}, {}, "Stream/Input/Audio", {}, {}, {}});
        }
        return result;
    }

    // Represent active Data2G streams as graph targets so the existing
    // voice-device rules also exclude the other half of each radio card.
    const auto deviceCount = nodes.size();
    for (size_t i = 0; i < deviceCount; ++i)
    {
        const auto& node = nodes[i];
        const bool input = ear::isSource(node);
        if (usesDevice(input ? radio.input : radio.output, node.device))
        {
            ear::GraphNode stream{static_cast<uint32_t>(nodes.size() + 1), 0, {}, {},
                                  input ? "Stream/Input/Audio" : "Stream/Output/Audio",
                                  "data2g-host", node.name, {}};
            nodes.push_back(std::move(stream));
        }
    }
    return result;
}

AudioStreamBasicDescription pcm(int rate)
{
    return {static_cast<double>(rate), kAudioFormatLinearPCM, kAudioFormatFlagsNativeFloatPacked,
            sizeof(float), 1, sizeof(float), 1, 32, 0};
}

OSStatus pinDevice(AudioQueueRef queue, const std::string& uid)
{
    CFStringRef value = CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(uid.data()),
                                               uid.size(), kCFStringEncodingUTF8, false);
    if (!value) return kAudio_ParamError;
    OSStatus error = AudioQueueSetProperty(queue, kAudioQueueProperty_CurrentDevice, &value, sizeof(value));
    CFRelease(value);
    return error;
}

std::string audioError(const char* action, OSStatus error)
{
    return std::string(action) + " (Core Audio error " + std::to_string(error) + ")";
}

// Used by both the Ear and voice. Owners call open and close on their
// state queue. Core Audio can drain its callbacks during close because
// they forward work asynchronously and never wait for the state queue.
class CoreAudioInput
{
public:
    using Error = std::function<void(OSStatus)>;

    CoreAudioInput(dispatch_queue_t state, dispatch_queue_t callbacks) : state_(state), callbacks_(callbacks) {}
    bool open() const { return input_ != nullptr; }

    OSStatus start(const std::string& device, int rate, VoiceAudio::Samples samples, Error failed)
    {
        auto format = pcm(rate);
        alive_ = std::make_shared<bool>(true);
        auto alive = alive_;
        auto state = state_;
        OSStatus error = AudioQueueNewInputWithDispatchQueue(&input_, &format, 0, callbacks_,
            ^(AudioQueueRef queue, AudioQueueBufferRef buffer, const AudioTimeStamp*, UInt32,
              const AudioStreamPacketDescription*) {
                dispatch_async(state, ^{
                    if (!*alive) return;
                    const auto count = buffer->mAudioDataByteSize / sizeof(float);
                    if (count) samples(static_cast<const float*>(buffer->mAudioData), count, rate);
                    if (!*alive) return;
                    OSStatus result = AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
                    if (result != noErr) failed(result);
                });
            });
        if (error == noErr) error = pinDevice(input_, device);
        for (int i = 0; i < 3 && error == noErr; ++i)
        {
            AudioQueueBufferRef buffer = nullptr;
            error = AudioQueueAllocateBuffer(input_, rate / 50 * sizeof(float), &buffer);
            if (error == noErr) error = AudioQueueEnqueueBuffer(input_, buffer, 0, nullptr);
        }
        if (error == noErr) error = AudioQueueStart(input_, nullptr);
        if (error != noErr) close();
        return error;
    }

    void close()
    {
        if (!input_) return;
        *alive_ = false;
        AudioQueueDispose(input_, true);
        input_ = nullptr;
    }

private:
    dispatch_queue_t state_;
    dispatch_queue_t callbacks_;
    AudioQueueRef input_ = nullptr;
    std::shared_ptr<bool> alive_;
};

class CoreAudioVoice final : public VoiceAudio
{
public:
    CoreAudioVoice() { dispatch_queue_set_specific(queue_, this, this, nullptr); }

    ~CoreAudioVoice() override
    {
        stop();
        dispatch_release(callbackQueue_);
        dispatch_release(queue_);
    }

    void start(Changed changed, const std::vector<std::string>& remembered) override
    {
        stop();
        sync(^{
            changed_ = changed;
            remembered_ = remembered;
            status_.available = true;
            refresh();
            timer_ = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue_);
            dispatch_source_set_timer(timer_, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 4),
                                      NSEC_PER_SEC / 4, NSEC_PER_SEC / 20);
            dispatch_source_set_event_handler(timer_, ^{ refresh(); });
            dispatch_resume(timer_);
        });
    }

    void stop() override
    {
        sync(^{
            if (timer_)
            {
                dispatch_source_cancel(timer_);
                dispatch_release(timer_);
                timer_ = nullptr;
            }
            changed_ = nullptr;
            samples_ = nullptr;
            closeInput();
            closeOutput(false);
            status_ = {};
            graphKeys_.clear();
        });
    }

    void chooseMicrophone(const std::string& name) override
    {
        sync(^{
            if (name == status_.chosenMicrophone) return;
            status_.chosenMicrophone = name;
            updatePicks();
        });
    }

    void chooseSpeaker(const std::string& name) override
    {
        sync(^{
            if (name == status_.chosenSpeaker) return;
            status_.chosenSpeaker = name;
            updatePicks();
        });
    }

    void forgetRadio() override
    {
        sync(^{
            remembered_.clear();
            graphKeys_.clear();
            refresh();
        });
    }

    void listen(Samples samples) override
    {
        sync(^{
            closeInput();
            samples_ = samples;
            refresh();
            if (!input_.open()) openInput();
        });
    }

    void stopListening() override
    {
        sync(^{
            samples_ = nullptr;
            closeInput();
        });
    }

    void play(std::vector<float> samples, int rate, Finished finished) override
    {
        // The block executes synchronously, so move from the caller's vector
        // via a pointer instead of copying the audio into the block.
        auto* source = &samples;
        sync(^{ beginOutput(std::move(*source), rate, true, finished); });
    }

    void playStream(int rate, Finished finished) override
    {
        sync(^{ beginOutput({}, rate, false, finished); });
    }

    void append(const float* samples, size_t n) override
    {
        sync(^{
            if (n && output_ && !endOutput_) outputSamples_.insert(outputSamples_.end(), samples, samples + n);
        });
    }

    void endStream() override { sync(^{ endOutput_ = true; }); }
    void stopPlaying() override { sync(^{ closeOutput(false); }); }
    void setVolume(float gain) override { sync(^{ gain_ = gain; }); }

    double playPosition() const override
    {
        __block double position = 0;
        sync(^{ position = outputRate_ ? static_cast<double>(sent_) / outputRate_ : 0; });
        return position;
    }

    VoiceStatus status() const override
    {
        __block VoiceStatus status;
        sync(^{ status = status_; });
        return status;
    }

private:
    void sync(dispatch_block_t block) const
    {
        if (dispatch_get_specific(this))
            block();
        else
            dispatch_sync(queue_, block);
    }

    void refresh()
    {
        auto snapshot = deviceGraph(true);
        if (snapshot.keys == graphKeys_) return;
        graphKeys_ = std::move(snapshot.keys);
        status_.devices = voice::devices(snapshot.graph, remembered_);
        remembered_ = voice::rememberRadio(remembered_, status_.devices.radioNow);
        defaultInput_ = std::move(snapshot.defaultInput);
        defaultOutput_ = std::move(snapshot.defaultOutput);
        updatePicks();
        if (changed_) changed_();
    }

    void updatePicks()
    {
        auto& devices = status_.devices;
        auto mic = voice::pickDevice(devices.microphones, devices, status_.chosenMicrophone, defaultInput_);
        auto speaker = voice::pickDevice(devices.speakers, devices, status_.chosenSpeaker, defaultOutput_);
        const bool microphoneChanged = mic.node.name != status_.microphone.node.name || mic.why != status_.microphone.why;
        const bool speakerChanged = speaker.node.name != status_.speaker.node.name || speaker.why != status_.speaker.why;
        status_.microphone = std::move(mic);
        status_.speaker = std::move(speaker);
        if (microphoneChanged)
        {
            closeInput();
            if (samples_) openInput();
        }
        if (speakerChanged && output_) closeOutput(false);
    }

    void closeInput()
    {
        input_.close();
        status_.listening = false;
        status_.recording = false;
        status_.microphoneRate = 0;
        status_.microphoneProblem.clear();
    }

    void openInput()
    {
        if (!status_.available || !status_.microphone.usable()) return;
        constexpr int rate = 48000;
        auto error = input_.start(status_.microphone.node.device, rate,
            [this](const float* data, size_t count, int sampleRate) {
                status_.recording = true;
                if (samples_) samples_(data, count, sampleRate);
            },
            [this](OSStatus error) { failInput(error); });
        if (error != noErr)
        {
            failInput(error);
            return;
        }
        status_.listening = true;
        status_.microphoneRate = rate;
    }

    void failInput(OSStatus error)
    {
        closeInput();
        status_.microphoneProblem = audioError("Could not record from the selected microphone", error);
    }

    void beginOutput(std::vector<float> samples, int rate, bool end, Finished finished)
    {
        closeOutput(false);
        status_.speakerProblem.clear();
        refresh();
        finished_ = std::move(finished);
        if (!status_.available || !status_.speaker.usable() || rate <= 0)
        {
            status_.speakerProblem = "Choose an available speaker.";
            closeOutput(false);
            return;
        }
        outputSamples_ = std::move(samples);
        outputRate_ = rate;
        endOutput_ = end;
        auto format = pcm(rate);
        outputAlive_ = std::make_shared<bool>(true);
        auto alive = outputAlive_;
        auto stateQueue = queue_;
        OSStatus error = AudioQueueNewOutputWithDispatchQueue(&output_, &format, 0, callbackQueue_,
            ^(AudioQueueRef, AudioQueueBufferRef buffer) {
                dispatch_async(stateQueue, ^{
                    if (!*alive) return;
                    --queued_;
                    const auto result = fillOutput(buffer);
                    if (result != noErr)
                        failOutput(result);
                    else if (queued_ == 0)
                        closeOutput(true);
                });
            });
        if (error == noErr) error = pinDevice(output_, status_.speaker.node.device);
        for (int i = 0; i < 3 && error == noErr; ++i)
        {
            AudioQueueBufferRef buffer = nullptr;
            error = AudioQueueAllocateBuffer(output_, std::max(1, rate / 50) * sizeof(float), &buffer);
            if (error == noErr) error = fillOutput(buffer);
        }
        if (error == noErr && queued_ == 0)
        {
            closeOutput(true);
            return;
        }
        if (error == noErr) error = AudioQueueStart(output_, nullptr);
        if (error != noErr)
        {
            failOutput(error);
            return;
        }
        status_.playing = true;
    }

    OSStatus fillOutput(AudioQueueBufferRef buffer)
    {
        auto* dest = static_cast<float*>(buffer->mAudioData);
        const auto capacity = buffer->mAudioDataBytesCapacity / sizeof(float);
        const auto count = std::min(capacity, outputSamples_.size() - read_);
        if (count == 0 && endOutput_) return noErr;
        if (count != 0)
            std::transform(outputSamples_.begin() + read_, outputSamples_.begin() + read_ + count, dest,
                           [gain = gain_](float sample) { return std::clamp(sample * gain, -1.f, 1.f); });
        if (!endOutput_) std::fill(dest + count, dest + capacity, 0.f);
        buffer->mAudioDataByteSize = (endOutput_ ? count : capacity) * sizeof(float);
        read_ += count;
        sent_ += count;
        if (read_ == outputSamples_.size())
        {
            outputSamples_.clear();
            read_ = 0;
        }
        const auto error = AudioQueueEnqueueBuffer(output_, buffer, 0, nullptr);
        if (error == noErr) ++queued_;
        return error;
    }

    void failOutput(OSStatus error)
    {
        status_.speakerProblem = audioError("Could not play through the selected speaker", error);
        closeOutput(false);
    }

    void closeOutput(bool played)
    {
        if (output_)
        {
            *outputAlive_ = false;
            AudioQueueDispose(output_, true);
            output_ = nullptr;
        }
        status_.playing = false;
        outputSamples_.clear();
        read_ = sent_ = 0;
        queued_ = outputRate_ = 0;
        endOutput_ = false;
        if (auto finished = std::exchange(finished_, {})) finished(played);
    }

    // Disposal waits for Core Audio's callbacks. They must return without
    // waiting for the state queue, which may be disposing their AudioQueue.
    // After retirement, forwarded callbacks check their token before using
    // the instance or a buffer, both of which may already have been freed.
    dispatch_queue_t callbackQueue_ = dispatch_queue_create("org.baitisj.ragtime.voice.callbacks", DISPATCH_QUEUE_SERIAL);
    dispatch_queue_t queue_ = dispatch_queue_create("org.baitisj.ragtime.voice", DISPATCH_QUEUE_SERIAL);
    dispatch_source_t timer_ = nullptr;
    VoiceStatus status_;
    Changed changed_;
    Samples samples_;
    Finished finished_;
    std::vector<std::string> remembered_;
    std::vector<std::string> graphKeys_;
    std::string defaultInput_, defaultOutput_;
    CoreAudioInput input_{queue_, callbackQueue_};
    AudioQueueRef output_ = nullptr;
    std::shared_ptr<bool> outputAlive_;
    std::vector<float> outputSamples_;
    size_t read_ = 0, sent_ = 0;
    int queued_ = 0, outputRate_ = 0;
    float gain_ = 1;
    bool endOutput_ = false;
};
class CoreAudioEar final : public EarAudio
{
public:
    ~CoreAudioEar() override
    {
        stop();
        dispatch_release(callbackQueue_);
        dispatch_release(queue_);
    }

    void start(Samples samples, Changed changed) override
    {
        stop();
        dispatch_sync(queue_, ^{
            samples_ = samples;
            changed_ = changed;
            status_.available = true;
            refresh();
            timer_ = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue_);
            dispatch_source_set_timer(timer_, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 2),
                                      NSEC_PER_SEC / 2, NSEC_PER_SEC / 10);
            dispatch_source_set_event_handler(timer_, ^{ refresh(); });
            dispatch_resume(timer_);
        });
    }

    void stop() override
    {
        dispatch_sync(queue_, ^{
            if (timer_)
            {
                dispatch_source_cancel(timer_);
                dispatch_release(timer_);
                timer_ = nullptr;
            }
            input_.close();
            activeSource_.clear();
            samples_ = {};
            changed_ = {};
            status_.available = false;
            status_.rate = 0;
        });
    }

    void choose(const std::string& name) override
    {
        dispatch_sync(queue_, ^{
            status_.chosen = name;
            updateSource();
        });
    }

    void setHostPid(std::optional<int> pid) override
    {
        dispatch_sync(queue_, ^{
            hostPid_ = pid;
            updateSource();
        });
    }

    void hostAudio(const std::string& name, const float* data, size_t count, int rate, bool tx) override
    {
        dispatch_sync(queue_, ^{
            if (tapName_ != name)
            {
                tapName_ = name;
                updateSource();
            }
            if (status_.available && status_.following && !tapName_.empty() && count)
                receive(data, count, rate, tx);
        });
    }

    EarStatus status() const override
    {
        __block EarStatus result;
        dispatch_sync(queue_, ^{
            result = status_;
            result.streaming = result.available && result.rate > 0
                               && Clock::now() - lastAudio_ < std::chrono::seconds(1);
        });
        return result;
    }

private:
    using Clock = std::chrono::steady_clock;

    void refresh()
    {
        devices_ = deviceGraph(false);
        status_.sources = ear::sources(devices_.graph);
        updateSource();
    }

    void updateSource()
    {
        if (!status_.available) return;
        status_.following = status_.chosen.empty();
        status_.host = ear::findHostCapture(devices_.graph, hostPid_);
        if (status_.host.found && status_.host.found->source != 0)
            lastHostSource_ = status_.host.found->sourceName;

        const bool tapping = status_.following && !tapName_.empty();
        const auto pick = ear::pickSource(devices_.graph, status_.chosen, status_.host,
                                          lastHostSource_, devices_.defaultInput);
        if (!tapName_.empty())
        {
            status_.host.candidates = 1;
            status_.host.found = ear::HostCapture{0, "Built-in Data2G", 0, 1, {}, tapName_, true};
            status_.host.why.clear();
        }
        status_.hostTap = tapping;
        status_.pick = tapping ? ear::Pick::Host : pick.why;
        status_.listening = tapping ? 1 : pick.node ? pick.node->id : 0;
        status_.listeningName = tapping ? std::string() : pick.node ? pick.node->name : std::string();
        status_.listeningDescription = tapping ? tapName_ : pick.node ? pick.node->description : std::string();

        const std::string source = tapping ? "modem:" + tapName_ : status_.listeningName;
        if (source == activeSource_) return;
        activeSource_ = source;
        input_.close();
        status_.rate = 0;
        lastAudio_ = {};
        status_.problem.clear();
        if (!tapping && pick.node)
        {
            constexpr int rate = 48000;
            OSStatus error = input_.start(pick.node->device, rate,
                [this](const float* data, size_t count, int sampleRate) { receive(data, count, sampleRate, std::nullopt); },
                [this](OSStatus error) { inputFailed(error); });
            if (error != noErr) inputFailed(error);
        }
        if (changed_) changed_();
    }

    void receive(const float* data, size_t count, int rate, std::optional<bool> tx)
    {
        lastAudio_ = Clock::now();
        status_.rate = rate;
        if (samples_) samples_(data, count, rate, tx);
    }

    void inputFailed(OSStatus error)
    {
        input_.close();
        status_.rate = 0;
        status_.problem = audioError("Could not record the selected input", error);
        if (changed_) changed_();
    }

    dispatch_queue_t queue_ = dispatch_queue_create("org.baitisj.ragtime.ear", DISPATCH_QUEUE_SERIAL);
    dispatch_queue_t callbackQueue_ = dispatch_queue_create("org.baitisj.ragtime.ear.callbacks", DISPATCH_QUEUE_SERIAL);
    CoreAudioInput input_{queue_, callbackQueue_};
    dispatch_source_t timer_ = nullptr;
    Clock::time_point lastAudio_;
    EarStatus status_;
    DeviceGraph devices_;
    Samples samples_;
    Changed changed_;
    std::optional<int> hostPid_;
    std::string lastHostSource_;
    std::string activeSource_;
    std::string tapName_;
};
} // namespace

void reserveCoreAudioRadioDevices(const std::string& input, const std::string& output)
{
    RadioDevices devices{radioUids(input, true), radioUids(output, false)};
    std::lock_guard<std::mutex> lock(radioMutex);
    reserved = std::move(devices);
}

void releaseCoreAudioRadioDevices()
{
    std::lock_guard<std::mutex> lock(radioMutex);
    reserved = {};
}

std::unique_ptr<EarAudio> makeEarAudio() { return std::make_unique<CoreAudioEar>(); }

std::unique_ptr<VoiceAudio> makeVoiceAudio() { return std::make_unique<CoreAudioVoice>(); }
} // namespace ragtime::audio
