#ifndef RAGTIME_MACOS_CORE_AUDIO_QUEUE_H
#define RAGTIME_MACOS_CORE_AUDIO_QUEUE_H

#include <AudioToolbox/AudioToolbox.h>
#include <chrono>
#include <cstdint>
#include <string>

namespace ragtime { namespace audio { namespace macos {

inline int64_t audioNowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline std::string audioError(const char* operation, OSStatus code)
{
    return std::string(operation) + " (Core Audio " + std::to_string(code) + ")";
}

inline AudioStreamBasicDescription monoFormat(int rate)
{
    AudioStreamBasicDescription f{};
    f.mSampleRate = rate;
    f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    f.mBytesPerPacket = f.mBytesPerFrame = sizeof(float);
    f.mFramesPerPacket = 1;
    f.mChannelsPerFrame = 1;
    f.mBitsPerChannel = 32;
    return f;
}

inline OSStatus pinQueue(AudioQueueRef queue, const std::string& uid)
{
    CFStringRef device = CFStringCreateWithBytes(nullptr, reinterpret_cast<const UInt8*>(uid.data()),
                          static_cast<CFIndex>(uid.size()), kCFStringEncodingUTF8, false);
    if (!device) return kAudio_ParamError;
    const auto result = AudioQueueSetProperty(queue, kAudioQueueProperty_CurrentDevice, &device, sizeof(device));
    CFRelease(device);
    return result;
}

} } }

#endif
