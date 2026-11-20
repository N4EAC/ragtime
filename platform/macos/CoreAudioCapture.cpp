#include "CoreAudioCapture.h"
#include "CoreAudioQueue.h"

#include <atomic>
#include <cmath>
#include <vector>

namespace ragtime { namespace audio { namespace macos {
namespace
{

bool shareableInput(const Device& device, std::string& problem)
{
    AudioObjectPropertyAddress address{kAudioDevicePropertyHogMode, kAudioObjectPropertyScopeGlobal,
                                        kAudioObjectPropertyElementMain};
    if (AudioObjectHasProperty(device.id, &address))
    {
        pid_t owner = -1;
        UInt32 bytes = sizeof(owner);
        const auto result = AudioObjectGetPropertyData(device.id, &address, 0, nullptr, &bytes, &owner);
        if (result != noErr)
        {
            problem = audioError("Could not verify shared access to the audio input", result);
            return false;
        }
        if (owner != -1)
        {
            problem = "Another audio stream has exclusive access to this input";
            return false;
        }
    }
    address.mSelector = kAudioDevicePropertyStreams;
    address.mScope = kAudioObjectPropertyScopeInput;
    UInt32 bytes = 0;
    auto result = AudioObjectGetPropertyDataSize(device.id, &address, 0, nullptr, &bytes);
    if (result != noErr || !bytes || bytes % sizeof(AudioStreamID))
    {
        problem = "The audio input's streams could not be verified";
        return false;
    }
    std::vector<AudioStreamID> streams(bytes / sizeof(AudioStreamID));
    result = AudioObjectGetPropertyData(device.id, &address, 0, nullptr, &bytes, streams.data());
    if (result != noErr)
    {
        problem = audioError("Could not read the audio input's streams", result);
        return false;
    }
    streams.resize(bytes / sizeof(AudioStreamID));
    address = {kAudioStreamPropertyVirtualFormat, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    for (auto stream : streams)
    {
        AudioStreamBasicDescription format{};
        bytes = sizeof(format);
        result = AudioObjectGetPropertyData(stream, &address, 0, nullptr, &bytes, &format);
        if (result != noErr || format.mFormatID != kAudioFormatLinearPCM ||
            (format.mFormatFlags & kAudioFormatFlagIsNonMixable))
        {
            problem = "The audio input is not using a shareable PCM format";
            return false;
        }
    }
    return true;
}

class QueueCapture : public InputCapture
{
public:
    ~QueueCapture() override
    {
        active = false;
        if (queue)
        {
            AudioQueueStop(queue, true);
            AudioQueueDispose(queue, true);
        }
    }

    int rate() const override { return sampleRate; }
    int64_t lastSamplesMs() const override { return lastSamples; }
    std::string problem() const override
    {
        if (failure != noErr) return audioError("Audio input stopped", failure);
        UInt32 running = 0, bytes = sizeof(running);
        const auto result = AudioQueueGetProperty(queue, kAudioQueueProperty_IsRunning, &running, &bytes);
        if (result != noErr) return audioError("Could not check the audio input", result);
        if (running) started = true;
        if (!running && (started || audioNowMs() - startMs > 3000)) return "The audio input stopped";
        return {};
    }

    static void receive(void* user, AudioQueueRef queue, AudioQueueBufferRef buffer,
                        const AudioTimeStamp*, UInt32, const AudioStreamPacketDescription*)
    {
        auto& c = *static_cast<QueueCapture*>(user);
        if (!c.active) return;
        const auto n = buffer->mAudioDataByteSize / sizeof(float);
        if (n)
        {
            c.lastSamples = audioNowMs();
            c.samples(static_cast<const float*>(buffer->mAudioData), n, c.sampleRate);
        }
        if (c.active)
        {
            const auto result = AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
            if (result != noErr) c.failure = result;
        }
    }

    CaptureSamples samples;
    int sampleRate = 0;
    AudioQueueRef queue = nullptr;
    std::atomic<bool> active{true};
    std::atomic<int64_t> lastSamples{0};
    std::atomic<OSStatus> failure{noErr};
    int64_t startMs = 0;
    mutable bool started = false;
};

} // namespace

std::unique_ptr<InputCapture> openInputCapture(const Device& device, unsigned bufferMs,
                                             CaptureSamples samples, std::string& problem)
{
    problem.clear();
    if (!std::isfinite(device.rate) || device.rate < 1000 || device.rate > 192000)
    {
        problem = "The audio input's sample rate is unavailable";
        return nullptr;
    }
    if (!samples || !device.inputs || device.uid.empty() || bufferMs < 10 || bufferMs > 1000)
    {
        problem = "Invalid audio input or capture buffer";
        return nullptr;
    }
    if (!shareableInput(device, problem)) return nullptr;
    auto c = std::make_unique<QueueCapture>();
    c->samples = std::move(samples);
    c->sampleRate = static_cast<int>(std::lround(device.rate));
    const auto f = monoFormat(c->sampleRate);
    auto result = AudioQueueNewInput(&f, QueueCapture::receive, c.get(), nullptr, nullptr, 0, &c->queue);
    if (result == noErr) result = pinQueue(c->queue, device.uid);
    for (int i = 0; result == noErr && i < 3; ++i)
    {
        AudioQueueBufferRef buffer = nullptr;
        const auto bytes = static_cast<UInt32>(static_cast<uint64_t>(c->sampleRate) * bufferMs / 1000 * sizeof(float));
        result = AudioQueueAllocateBuffer(c->queue, bytes, &buffer);
        if (result == noErr) result = AudioQueueEnqueueBuffer(c->queue, buffer, 0, nullptr);
    }
    c->startMs = audioNowMs();
    if (result == noErr) result = AudioQueueStart(c->queue, nullptr);
    if (result != noErr)
    {
        problem = audioError("Could not start the audio input", result);
        return nullptr;
    }
    return c;
}

} } }
