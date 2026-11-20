#ifndef RAGTIME_MACOS_VOICE_BUFFER_H
#define RAGTIME_MACOS_VOICE_BUFFER_H

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <vector>

namespace ragtime
{
namespace audio
{
namespace macos
{

// The queue callback never waits for an arriving network chunk. Gaps are
// silence and do not advance the source playhead. Tested without hardware.
class VoiceBuffer
{
public:
    VoiceBuffer(std::vector<float> samples, bool streaming)
        : samples_(std::move(samples)), open_(streaming) {}

    void append(const float* samples, std::size_t n)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!open_ || !n) return;
        // Compact on the producer, never on the audio callback.
        if (at_)
        {
            samples_.erase(samples_.begin(), samples_.begin() + static_cast<std::ptrdiff_t>(at_));
            at_ = 0;
        }
        samples_.insert(samples_.end(), samples, samples + n);
    }

    void end()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        open_ = false;
    }

    std::size_t fill(float* out, std::size_t capacity, float gain = 1.0f)
    {
        std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
        if (!lock.owns_lock())
        {
            std::fill_n(out, capacity, 0.0f);
            return capacity;
        }
        const auto n = std::min(capacity, samples_.size() - at_);
        if (n) std::copy_n(samples_.data() + at_, n, out);
        for (std::size_t i = 0; i < n; ++i) out[i] *= gain;
        at_ += n;
        position.fetch_add(n);
        if (at_ == samples_.size() && !open_) eof = true;
        if (open_)
        {
            std::fill(out + n, out + capacity, 0.0f);
            return capacity;
        }
        return n;
    }

    std::atomic<std::size_t> position{0};
    std::atomic<bool> eof{false};

private:
    std::mutex mutex_;
    std::vector<float> samples_;
    std::size_t at_ = 0;
    bool open_;
};

} // namespace macos
} // namespace audio
} // namespace ragtime

#endif
