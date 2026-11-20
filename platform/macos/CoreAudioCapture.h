#ifndef RAGTIME_MACOS_CORE_AUDIO_CAPTURE_H
#define RAGTIME_MACOS_CORE_AUDIO_CAPTURE_H

#include "CoreAudioDevices.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace ragtime { namespace audio { namespace macos {

using CaptureSamples = std::function<void(const float*, std::size_t, int)>;

// Each caller owns a separate, device-pinned queue. Destruction stops and
// disposes the queue synchronously; no sample callbacks follow it.
class InputCapture
{
public:
    virtual ~InputCapture() = default;
    virtual int rate() const = 0;
    virtual int64_t lastSamplesMs() const = 0;
    virtual std::string problem() const = 0;
};

// Permission is checked by the caller before opening. No default-device,
// hardware-rate, hardware-buffer-size or exclusive-access setters.
std::unique_ptr<InputCapture> openInputCapture(const Device& device, unsigned bufferMs,
                                             CaptureSamples samples, std::string& problem);

} } }

#endif
