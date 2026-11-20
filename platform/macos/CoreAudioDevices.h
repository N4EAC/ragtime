#ifndef RAGTIME_MACOS_CORE_AUDIO_DEVICES_H
#define RAGTIME_MACOS_CORE_AUDIO_DEVICES_H

#include <cstdint>
#include <string>
#include <vector>

#include "ragtime/ear/Graph.h"

namespace ragtime
{
namespace audio
{
namespace macos
{

// Plain snapshots keep radio isolation testable without opening hardware.
struct Device
{
    uint32_t id = 0;
    std::string uid;
    std::string label;
    unsigned inputs = 0;
    unsigned outputs = 0;
    double rate = 0;
    // Related physical devices and aggregate members belong to one card
    // for exclusion purposes. Never group unrelated devices by model name.
    std::vector<uint32_t> related;
};

struct Process
{
    int pid = 0;
    std::string program;
    std::vector<uint32_t> inputs;
    std::vector<uint32_t> outputs;
};

struct DeviceSnapshot
{
    std::vector<Device> devices;
    std::vector<Process> processes;
    uint32_t defaultInput = 0;
    uint32_t defaultOutput = 0;
    std::string problem;
};

// These queries neither open streams nor change any hardware properties.
// Voice stays unavailable if process/device ownership cannot be verified.
DeviceSnapshot readDevices();
ear::Graph deviceGraph(const DeviceSnapshot& snapshot);
std::string deviceName(const std::string& uid, bool input);
std::string defaultName(const DeviceSnapshot& snapshot, bool input);
const Device* findDevice(const DeviceSnapshot& snapshot, const std::string& name, bool input);

} // namespace macos
} // namespace audio
} // namespace ragtime

#endif
