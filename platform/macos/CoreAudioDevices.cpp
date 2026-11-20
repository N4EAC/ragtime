#include "CoreAudioDevices.h"

#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <libproc.h>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace ragtime
{
namespace audio
{
namespace macos
{
namespace
{

AudioObjectPropertyAddress address(AudioObjectPropertySelector selector,
                                   AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
{
    return {selector, scope, kAudioObjectPropertyElementMain};
}

template <typename T>
bool value(AudioObjectID id, AudioObjectPropertyAddress a, T& result)
{
    UInt32 bytes = sizeof(T);
    return AudioObjectGetPropertyData(id, &a, 0, nullptr, &bytes, &result) == noErr && bytes == sizeof(T);
}

template <typename T>
bool list(AudioObjectID id, AudioObjectPropertyAddress a, std::vector<T>& result)
{
    UInt32 bytes = 0;
    if (AudioObjectGetPropertyDataSize(id, &a, 0, nullptr, &bytes) != noErr || bytes % sizeof(T)) return false;
    result.resize(bytes / sizeof(T));
    if (bytes && AudioObjectGetPropertyData(id, &a, 0, nullptr, &bytes, result.data()) != noErr) return false;
    result.resize(bytes / sizeof(T));
    return true;
}

std::string stringValue(AudioObjectID id, AudioObjectPropertySelector selector)
{
    CFStringRef text = nullptr;
    if (!value(id, address(selector), text) || !text) return {};
    std::vector<char> utf8(static_cast<std::size_t>(CFStringGetMaximumSizeForEncoding(
                                 CFStringGetLength(text), kCFStringEncodingUTF8)) + 1);
    const bool ok = CFStringGetCString(text, utf8.data(), static_cast<CFIndex>(utf8.size()), kCFStringEncodingUTF8);
    CFRelease(text);
    return ok ? std::string(utf8.data()) : std::string();
}

bool channels(AudioObjectID id, AudioObjectPropertyScope scope, unsigned& count)
{
    const auto a = address(kAudioDevicePropertyStreamConfiguration, scope);
    UInt32 bytes = 0;
    if (AudioObjectGetPropertyDataSize(id, &a, 0, nullptr, &bytes) != noErr) return false;
    if (bytes < offsetof(AudioBufferList, mBuffers)) return false;
    // Aligned storage, including room for the variable length buffer list.
    std::vector<std::max_align_t> storage((bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    if (AudioObjectGetPropertyData(id, &a, 0, nullptr, &bytes, storage.data()) != noErr) return false;
    const auto* buffers = reinterpret_cast<const AudioBufferList*>(storage.data());
    if (buffers->mNumberBuffers > (bytes - offsetof(AudioBufferList, mBuffers)) / sizeof(AudioBuffer)) return false;
    count = 0;
    for (UInt32 i = 0; i < buffers->mNumberBuffers; ++i) count += buffers->mBuffers[i].mNumberChannels;
    return true;
}

std::string programName(pid_t pid, AudioObjectID process)
{
    char path[PROC_PIDPATHINFO_MAXSIZE] = {};
    std::string name;
    if (proc_pidpath(pid, path, sizeof(path)) > 0)
    {
        name = path;
        name = name.substr(name.find_last_of('/') + 1);
    }
    if (name.empty() && proc_name(pid, path, sizeof(path)) > 0) name = path;
    // A bundled host may have an alternate executable name.
    const auto bundle = stringValue(process, kAudioProcessPropertyBundleID);
    const auto component = bundle.substr(bundle.find_last_of('.') + 1);
    if (ear::isData2GProgram(component)) return component;
    return name;
}

} // namespace

DeviceSnapshot readDevices()
{
    DeviceSnapshot s;
    std::vector<AudioObjectID> ids, processes;
    const auto processList = address(kAudioHardwarePropertyProcessObjectList);
    if (!AudioObjectHasProperty(kAudioObjectSystemObject, &processList))
    {
        s.problem = "Voice needs macOS 14.2 or later to identify the radio's audio devices";
        return s;
    }
    if (!list(kAudioObjectSystemObject, address(kAudioHardwarePropertyDevices), ids) ||
        !list(kAudioObjectSystemObject, processList, processes))
    {
        s.problem = "Core Audio device ownership could not be read; voice is paused";
        return s;
    }
    value(kAudioObjectSystemObject, address(kAudioHardwarePropertyDefaultInputDevice), s.defaultInput);
    value(kAudioObjectSystemObject, address(kAudioHardwarePropertyDefaultOutputDevice), s.defaultOutput);
    for (auto id : ids)
    {
        Device d;
        d.id = id;
        d.uid = stringValue(id, kAudioDevicePropertyDeviceUID);
        d.label = stringValue(id, kAudioObjectPropertyName);
        if (d.uid.empty() || !channels(id, kAudioObjectPropertyScopeInput, d.inputs) ||
            !channels(id, kAudioObjectPropertyScopeOutput, d.outputs))
        {
            s.problem = "Core Audio devices changed during discovery; voice is paused";
            return s;
        }
        value(id, address(kAudioDevicePropertyNominalSampleRate), d.rate);
        const AudioObjectPropertySelector relations[] = {
            kAudioDevicePropertyRelatedDevices, kAudioAggregateDevicePropertyActiveSubDeviceList};
        for (auto selector : relations)
        {
            const auto a = address(selector);
            if (!AudioObjectHasProperty(id, &a)) continue;
            std::vector<AudioObjectID> related;
            if (!list(id, a, related))
            {
                s.problem = "Core Audio device relationships could not be read; voice is paused";
                return s;
            }
            d.related.insert(d.related.end(), related.begin(), related.end());
        }
        s.devices.push_back(std::move(d));
    }
    for (auto id : processes)
    {
        Process p;
        if (!value(id, address(kAudioProcessPropertyPID), p.pid) ||
            !list(id, address(kAudioProcessPropertyDevices, kAudioObjectPropertyScopeInput), p.inputs) ||
            !list(id, address(kAudioProcessPropertyDevices, kAudioObjectPropertyScopeOutput), p.outputs))
        {
            s.problem = "Core Audio processes changed during discovery; voice is paused";
            return s;
        }
        if (p.inputs.empty() && p.outputs.empty()) continue;
        p.program = programName(p.pid, id);
        if (p.program.empty())
        {
            s.problem = "An audio device's owning process could not be identified; voice is paused";
            return s;
        }
        if (ear::isData2GProgram(p.program))
        {
            for (bool input : {true, false})
                for (auto used : input ? p.inputs : p.outputs)
                {
                    const auto device = std::find_if(s.devices.begin(), s.devices.end(), [used, input](const Device& d) {
                        return d.id == used && (input ? d.inputs : d.outputs);
                    });
                    if (device == s.devices.end())
                    {
                        s.problem = "A radio audio device could not be resolved; voice is paused";
                        return s;
                    }
                }
        }
        s.processes.push_back(std::move(p));
    }
    return s;
}

} // namespace macos
} // namespace audio
} // namespace ragtime
