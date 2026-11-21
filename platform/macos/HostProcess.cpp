#include "ragtime/audio/EarAudio.h"

namespace ragtime
{
namespace audio
{

std::optional<int> processListeningOn(int)
{
    // Command-port-to-PID lookup is not implemented. Audio process/device
    // discovery is independent; never infer a device from a remote port.
    return std::nullopt;
}

} // namespace audio
} // namespace ragtime
