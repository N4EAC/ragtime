#include "CoreAudioCapture.h"
#include "CoreAudioQueue.h"

#include <atomic>
#include <cmath>

namespace ragtime { namespace audio { namespace macos {
namespace
{

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
        return failure == noErr ? std::string() : audioError("Audio input stopped", failure);
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
        if (c.active) c.failure = AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
    }

    CaptureSamples samples;
    int sampleRate = 0;
    AudioQueueRef queue = nullptr;
    std::atomic<bool> active{true};
    std::atomic<int64_t> lastSamples{0};
    std::atomic<OSStatus> failure{noErr};
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
    if (result == noErr) result = AudioQueueStart(c->queue, nullptr);
    if (result != noErr)
    {
        problem = audioError("Could not start the audio input", result);
        return nullptr;
    }
    return c;
}

} } }
