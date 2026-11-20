#include "ragtime/audio/EarAudio.h"

namespace ragtime
{
namespace audio
{

std::optional<int> processListeningOn(int)
{
    // No native host/audio-device discovery yet. A remote TCP connection
    // does not identify a local radio device; never probe Linux's /proc.
    return std::nullopt;
}

} // namespace audio
} // namespace ragtime
