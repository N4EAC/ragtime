#ifndef RAGTIME_AUDIO_CORE_AUDIO_VOICE_H
#define RAGTIME_AUDIO_CORE_AUDIO_VOICE_H

#include <string>

namespace ragtime::audio
{
// Reserve the Core Audio devices the built-in modem is opening. An empty
// name means the current system default for that direction.
void reserveCoreAudioRadioDevices(const std::string& input, const std::string& output);
void releaseCoreAudioRadioDevices();
}

#endif
