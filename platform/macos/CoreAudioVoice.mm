// Native voice only. The Ear remains a separate backend: it must never
// share the operator microphone or follow the speaker's default route.
#include "ragtime/audio/VoiceAudio.h"
#include "CoreAudioDevices.h"
#include "CoreAudioCapture.h"
#include "CoreAudioQueue.h"
#include "VoiceBuffer.h"
#include "MicrophonePermission.h"

#import <Foundation/Foundation.h>
#include <AudioToolbox/AudioToolbox.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <future>
#include <mutex>
#include <sstream>
#include <thread>

namespace ragtime
{
namespace audio
{
namespace
{

using Clock = std::chrono::steady_clock;

using macos::audioNowMs;
using macos::audioError;
using macos::monoFormat;
using macos::pinQueue;

struct Playback
{
    macos::VoiceBuffer buffer;
    VoiceAudio::Finished finished;
    int rate;
    std::string name;
    AudioQueueRef queue = nullptr;
    std::atomic<OSStatus> failure{noErr};
    std::atomic<bool> active{true};
    std::atomic<bool> started{false};
    std::atomic<bool> tailQueued{false};
    std::atomic<float> gain{1.0f};
    bool draining = false;
    Clock::time_point startTime;

    Playback(std::vector<float> samples, bool stream, int r, VoiceAudio::Finished done)
        : buffer(std::move(samples), stream), finished(std::move(done)), rate(r) {}

    ~Playback()
    {
        active = false;
        if (queue)
        {
            AudioQueueStop(queue, true);
            AudioQueueDispose(queue, true);
        }
    }

    static void running(void* user, AudioQueueRef queue, AudioQueuePropertyID)
    {
        auto& p = *static_cast<Playback*>(user);
        UInt32 running = 0, bytes = sizeof(running);
        if (AudioQueueGetProperty(queue, kAudioQueueProperty_IsRunning, &running, &bytes) == noErr && running)
            p.started = true;
    }

    static void supply(void* user, AudioQueueRef queue, AudioQueueBufferRef buffer)
    {
        auto& p = *static_cast<Playback*>(user);
        if (!p.active) return;
        // Audio Queue's volume parameter accepts only 0..1; the UI also
        // supports boost. Apply gain to PCM as the other backends do.
        const auto n = p.buffer.fill(static_cast<float*>(buffer->mAudioData),
                                    buffer->mAudioDataBytesCapacity / sizeof(float), p.gain.load());
        if (n)
        {
            buffer->mAudioDataByteSize = static_cast<UInt32>(n * sizeof(float));
            p.failure = AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
        }
        // Publish EOF only after the final enqueue, so the worker cannot
        // start draining while that last buffer is still being submitted.
        if (p.buffer.eof) p.tailQueued = true;
    }
};

// Values have no UI or backend pointer: the authorization completion may
// arrive after stop() or destruction without calling into either.
struct Authorization
{
    std::atomic<bool> pending{false};
    std::atomic<bool> completed{false};
};

std::string signature(const VoiceStatus& s)
{
    std::ostringstream o;
    o << s.available << s.problem << s.chosenMicrophone << s.chosenSpeaker
      << static_cast<int>(s.microphone.why) << s.microphone.node.name
      << static_cast<int>(s.speaker.why) << s.speaker.node.name
      << s.listening << s.recording << s.microphonePermissionPending << s.microphoneRate << s.microphoneProblem
      << s.playing << s.speakerProblem;
    for (const auto& n : s.devices.microphones) o << '\n' << n.name << n.description;
    for (const auto& n : s.devices.speakers) o << '\n' << n.name << n.description;
    for (const auto& r : s.devices.radio) o << '\n' << r.node.name << r.inUse << r.sameCard << r.remembered;
    return o.str();
}

class CoreAudioVoice : public VoiceAudio
{
public:
    ~CoreAudioVoice() override { stop(); }

    void start(Changed changed, const std::vector<std::string>& remembered) override
    {
        stop();
        changed_ = std::move(changed);
        remembered_ = remembered;
        state_ = {};
        snapshot_ = {};
        lastSignature_.clear();
        authorization_ = std::make_shared<Authorization>();
        scan_ = true;
        {
            std::lock_guard<std::mutex> lock(commandsMutex_);
            quit_ = false;
            accepting_ = true;
        }
        worker_ = std::thread([this] { run(); });
    }

    void stop() override
    {
        {
            std::lock_guard<std::mutex> lock(commandsMutex_);
            accepting_ = false;
            quit_ = true;
        }
        wake_.notify_one();
        if (worker_.joinable()) worker_.join();
        changed_ = nullptr;
    }

    void chooseMicrophone(const std::string& name) override
    {
        post([this, name] { state_.chosenMicrophone = name; scan_ = true; retryCapture_ = true; });
    }
    void chooseSpeaker(const std::string& name) override
    {
        post([this, name] { state_.chosenSpeaker = name; scan_ = true; });
    }
    void forgetRadio() override
    {
        post([this] { remembered_.clear(); scan_ = true; });
    }
    void listen(Samples samples) override
    {
        post([this, samples = std::move(samples)] {
            capture_.reset();
            samples_ = samples;
            retryCapture_ = true;
            scan_ = true;
        });
    }
    void stopListening() override
    {
        auto done = std::make_shared<std::promise<void>>();
        auto completed = done->get_future();
        if (post([this, done] {
                samples_ = nullptr;
                capture_.reset();
                state_.microphonePermissionPending = false;
                state_.microphoneProblem.clear();
                done->set_value();
            })) completed.wait();
    }
    void play(std::vector<float> samples, int rate, Finished finished) override
    {
        begin(std::move(samples), false, rate, std::move(finished));
    }
    void playStream(int rate, Finished finished) override
    {
        begin({}, true, rate, std::move(finished));
    }
    void append(const float* samples, std::size_t n) override
    {
        if (!samples || !n) return;
        std::vector<float> copy(samples, samples + n);
        post([this, copy = std::move(copy)] {
            if (playback_) playback_->buffer.append(copy.data(), copy.size());
        });
    }
    void endStream() override
    {
        post([this] { if (playback_) playback_->buffer.end(); });
    }
    void stopPlaying() override { post([this] { finish(false); }); }
    double playPosition() const override { return position_; }
    void setVolume(float gain) override
    {
        if (!std::isfinite(gain)) return;
        post([this, gain] {
            volume_ = std::max(0.0f, gain);
            if (playback_) playback_->gain = volume_;
        });
    }
    VoiceStatus status() const override
    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        return published_;
    }

private:
    bool post(std::function<void()> command)
    {
        {
            std::lock_guard<std::mutex> lock(commandsMutex_);
            if (!accepting_) return false;
            commands_.push_back(std::move(command));
        }
        wake_.notify_one();
        return true;
    }

    void begin(std::vector<float> samples, bool stream, int rate, Finished finished)
    {
        // Preserve the completion if the backend has already stopped.
        const auto rejected = finished;
        if (!post([this, samples = std::move(samples), stream, rate, finished = std::move(finished)]() mutable {
                finish(false);
                state_.speakerProblem.clear();
                if (rate < 1000 || rate > 192000 || (!stream && samples.empty()))
                {
                    state_.speakerProblem = "No playable voice samples or invalid sample rate";
                    if (finished) finished(false);
                    return;
                }
                playback_ = std::make_unique<Playback>(std::move(samples), stream, rate, std::move(finished));
                position_ = 0;
                scan_ = true;
            }) && rejected) rejected(false);
    }

    void finish(bool played)
    {
        if (!playback_) return;
        position_ = static_cast<double>(playback_->buffer.position.load()) / playback_->rate;
        auto done = std::move(playback_->finished);
        playback_.reset(); // Dispose and synchronize callbacks before finishing.
        if (done) done(played);
    }

    void refresh()
    {
        snapshot_ = macos::readDevices();
        state_.available = snapshot_.problem.empty();
        state_.problem = snapshot_.problem;
        const auto graph = macos::deviceGraph(snapshot_);
        remembered_ = macos::rememberRelated(graph, remembered_);
        state_.devices = voice::devices(graph, remembered_);
        remembered_ = voice::rememberRadio(remembered_, state_.devices.radioNow);
        state_.microphone = voice::pickDevice(state_.devices.microphones, state_.devices,
                                    state_.chosenMicrophone, macos::defaultName(snapshot_, true));
        state_.speaker = voice::pickDevice(state_.devices.speakers, state_.devices,
                                    state_.chosenSpeaker, macos::defaultName(snapshot_, false));
        retryCapture_ = true;
    }

    bool authorized()
    {
        const auto auth = macos::microphonePermission();
        if (auth == macos::MicrophonePermission::Authorized) return true;
        if (auth == macos::MicrophonePermission::NotDetermined)
        {
            // Command-line probes have no usage description. Requesting
            // access from such a process would terminate it on macOS.
            auto permission = authorization_;
            if (!permission->pending.exchange(true) &&
                !macos::requestMicrophonePermission([permission] { permission->completed = true; }))
            {
                permission->pending = false;
                state_.microphoneProblem = "Microphone permission must be requested from the Ragtime app bundle";
            }
            else
            {
                state_.microphoneProblem.clear();
                state_.microphonePermissionPending = true;
            }
        }
        else state_.microphoneProblem = "Microphone access is denied; enable Ragtime in System Settings > Privacy & Security > Microphone";
        return false;
    }

    void openCapture(const macos::Device& device)
    {
        retryCapture_ = false;
        capture_ = macos::openInputCapture(device, 50, samples_, state_.microphoneProblem);
        captureName_ = capture_ ? state_.microphone.node.name : "";
    }

    void openPlayback(const macos::Device& device)
    {
        auto& p = *playback_;
        p.name = state_.speaker.node.name;
        p.gain = volume_;
        const auto f = monoFormat(p.rate);
        auto result = AudioQueueNewOutput(&f, Playback::supply, &p, nullptr, nullptr, 0, &p.queue);
        if (result == noErr) result = pinQueue(p.queue, device.uid);
        if (result == noErr) result = AudioQueueAddPropertyListener(p.queue, kAudioQueueProperty_IsRunning, Playback::running, &p);
        for (int i = 0; result == noErr && i < 3; ++i)
        {
            AudioQueueBufferRef buffer = nullptr;
            result = AudioQueueAllocateBuffer(p.queue, static_cast<UInt32>(p.rate / 20 * sizeof(float)), &buffer);
            if (result == noErr)
            {
                Playback::supply(&p, p.queue, buffer);
                result = p.failure;
            }
        }
        if (result == noErr) result = AudioQueueStart(p.queue, nullptr);
        if (result != noErr)
        {
            state_.speakerProblem = audioError("Could not start the speaker", result);
            finish(false);
        }
        else p.startTime = Clock::now();
    }

    void pump()
    {
        state_.microphonePermissionPending = false;
        if (authorization_->completed.exchange(false)) retryCapture_ = true;
        const auto* mic = macos::findDevice(snapshot_, state_.microphone.node.name, true);
        const bool useMic = samples_ && state_.available && state_.microphone.usable() && mic;
        if (capture_ && (!useMic || captureName_ != state_.microphone.node.name ||
                         std::fabs(mic->rate - capture_->rate()) > 0.5 ||
                         macos::microphonePermission() != macos::MicrophonePermission::Authorized))
            capture_.reset();
        if (useMic && !capture_ && retryCapture_ && authorized()) openCapture(*mic);
        if (samples_ && !useMic) state_.microphoneProblem = state_.available
            ? voice::describe(state_.microphone, true) : state_.problem;
        if (capture_)
        {
            const auto problem = capture_->problem();
            if (!problem.empty())
            {
                state_.microphoneProblem = problem;
                capture_.reset();
            }
        }
        state_.listening = capture_ != nullptr;
        state_.recording = capture_ && capture_->lastSamplesMs() && audioNowMs() - capture_->lastSamplesMs() < 1000;
        state_.microphoneRate = capture_ ? capture_->rate() : 0;

        if (playback_)
        {
            const auto* speaker = macos::findDevice(snapshot_, state_.speaker.node.name, false);
            if (!state_.available || !state_.speaker.usable() || !speaker ||
                (!playback_->name.empty() && playback_->name != state_.speaker.node.name))
            {
                state_.speakerProblem = state_.available ? voice::describe(state_.speaker, false) : state_.problem;
                finish(false);
            }
            else if (!playback_->queue) openPlayback(*speaker);
        }
        if (playback_ && playback_->queue)
        {
            auto& p = *playback_;
            position_ = static_cast<double>(p.buffer.position.load()) / p.rate;
            UInt32 running = 0, bytes = sizeof(running);
            auto result = AudioQueueGetProperty(p.queue, kAudioQueueProperty_IsRunning, &running, &bytes);
            if (running) p.started = true;
            if (result == noErr) result = p.failure;
            if (result == noErr && p.started && p.tailQueued && !p.draining)
            {
                p.active = false;
                result = AudioQueueStop(p.queue, false); // Drain queued samples before completion.
                p.draining = result == noErr;
            }
            if (result != noErr)
            {
                state_.speakerProblem = audioError("Speaker playback stopped", result);
                finish(false);
            }
            else if (p.draining && !running) finish(true);
            else if ((!p.started && Clock::now() - p.startTime > std::chrono::seconds(3)) ||
                     (p.started && !running && !p.draining))
            {
                state_.speakerProblem = "The selected speaker stopped before playback completed";
                finish(false);
            }
        }
        state_.playing = playback_ != nullptr;
    }

    void publish()
    {
        const auto key = signature(state_);
        if (key == lastSignature_) return;
        lastSignature_ = key;
        {
            std::lock_guard<std::mutex> lock(statusMutex_);
            published_ = state_;
        }
        if (changed_) changed_();
    }

    void run()
    {
        auto nextScan = Clock::now();
        for (;;)
        {
            @autoreleasepool
            {
                std::deque<std::function<void()>> commands;
                bool quit;
                {
                    std::unique_lock<std::mutex> lock(commandsMutex_);
                    wake_.wait_for(lock, std::chrono::milliseconds(20), [this] { return quit_ || !commands_.empty(); });
                    commands.swap(commands_);
                    quit = quit_;
                }
                for (auto& command : commands) command();
                if (quit) break;
                if (scan_ || Clock::now() >= nextScan)
                {
                    refresh();
                    scan_ = false;
                    nextScan = Clock::now() + std::chrono::milliseconds(750);
                }
                pump();
                publish();
            }
        }
        capture_.reset();
        samples_ = nullptr;
        finish(false);
        state_.listening = state_.recording = state_.playing = false;
        state_.microphonePermissionPending = false;
        state_.microphoneRate = 0;
        state_.available = false;
        publish();
    }

    std::mutex commandsMutex_;
    std::condition_variable wake_;
    std::deque<std::function<void()>> commands_;
    bool accepting_ = false;
    bool quit_ = false;
    std::thread worker_;
    mutable std::mutex statusMutex_;
    VoiceStatus published_;
    VoiceStatus state_;
    std::string lastSignature_;
    Changed changed_;
    std::vector<std::string> remembered_;
    macos::DeviceSnapshot snapshot_;
    bool scan_ = true;
    bool retryCapture_ = false;
    Samples samples_;
    std::unique_ptr<macos::InputCapture> capture_;
    std::string captureName_;
    std::unique_ptr<Playback> playback_;
    std::shared_ptr<Authorization> authorization_;
    std::atomic<double> position_{0};
    float volume_ = 1;
};

} // namespace

std::unique_ptr<VoiceAudio> makeVoiceAudio()
{
    return std::make_unique<CoreAudioVoice>();
}

} // namespace audio
} // namespace ragtime
