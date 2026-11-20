#ifndef RAGTIME_MACOS_MICROPHONE_PERMISSION_H
#define RAGTIME_MACOS_MICROPHONE_PERMISSION_H

#include <functional>

namespace ragtime { namespace audio { namespace macos {

enum class MicrophonePermission { Authorized, NotDetermined, Denied };
MicrophonePermission microphonePermission();
// False when the process has no bundle usage description. macOS would
// terminate it if it tried to request access (e.g. a command-line probe).
bool requestMicrophonePermission(std::function<void()> completed);

} } }

#endif
