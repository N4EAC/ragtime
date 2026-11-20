#include "Check.h"
#include "CoreAudioDevices.h"
#include "ragtime/voice/Devices.h"

#include <algorithm>

using namespace ragtime;
namespace mac = ragtime::audio::macos;

namespace
{

mac::DeviceSnapshot station()
{
    mac::DeviceSnapshot s;
    s.devices = {
        {10, "built-in", "Built in", 1, 2, 48000, {}},
        {20, "radio-in", "USB codec input", 2, 0, 48000, {21}},
        {21, "radio-out", "USB codec output", 0, 2, 48000, {20}},
        {30, "headset", "Headset", 1, 2, 44100, {}},
        {40, "aggregate", "Aggregate", 4, 4, 48000, {20, 30}},
    };
    s.defaultInput = 20;
    s.defaultOutput = 10;
    s.processes = {{123, "data2g-host", {20}, {}}};
    return s;
}

void isolation()
{
    auto s = station();
    auto d = voice::devices(mac::deviceGraph(s), {});
    // Input and output halves, related devices and aggregate constituents
    // are all excluded, even when the host opens only one input.
    CHECK_EQ(d.microphones.size(), 1u);
    CHECK_EQ(d.speakers.size(), 1u);
    CHECK_EQ(d.radio.size(), 6u);
    auto mic = voice::pickDevice(d.microphones, d, "", mac::defaultName(s, true));
    CHECK(mic.why == voice::Pick::InsteadOfRadio);
    CHECK_EQ(mic.node.name, mac::deviceName("built-in", true));
    CHECK(voice::pickDevice(d.speakers, d, mac::deviceName("radio-out", false), "").why == voice::Pick::Refused);
    CHECK(voice::pickDevice(d.speakers, d, "unplugged", "").why == voice::Pick::Missing);

    const auto remembered = d.radioNow;
    s.processes.clear();
    d = voice::devices(mac::deviceGraph(s), remembered);
    CHECK_EQ(d.radio.size(), 6u);
    CHECK(d.radioNow.empty());
    CHECK_EQ(voice::devices(mac::deviceGraph(s), {}).radio.size(), 0u);
    // A new aggregate involving a remembered radio remains excluded even
    // though the aggregate's own UID was never seen while the host ran.
    const auto graph = mac::deviceGraph(s);
    d = voice::devices(graph, mac::rememberRelated(graph, {mac::deviceName("radio-in", true)}));
    CHECK_EQ(d.radio.size(), 6u);

    // Embedded host GUI, and multiple hosts using different cards.
    s.processes = {{1, "data2g-gui", {}, {21}}, {2, "data2g-host", {10}, {}}};
    d = voice::devices(mac::deviceGraph(s), {});
    CHECK(d.microphones.empty());
    CHECK(d.speakers.empty());
    s.processes[0].program = "music";
    s.processes.resize(1);
    CHECK(voice::devices(mac::deviceGraph(s), {}).radio.empty());
}

void namesAndFailures()
{
    auto s = station();
    CHECK_EQ(mac::deviceName("a;%b", true), std::string("coreaudio:input:a%3B%25b"));
    const auto name = mac::deviceName("radio-out", false);
    CHECK(mac::findDevice(s, name, false) != nullptr);
    CHECK(mac::findDevice(s, name, true) == nullptr);
    // Persisted names follow UIDs, not transient Core Audio IDs.
    s.devices[2].id = 999;
    CHECK(mac::findDevice(s, name, false) == &s.devices[2]);
    s.defaultInput = 9999;
    CHECK(mac::defaultName(s, true).empty());
    s.problem = "ownership unavailable";
    CHECK(mac::deviceGraph(s).nodes.empty());
    s = station();
    s.processes[0].inputs = {99999};
    CHECK(mac::deviceGraph(s).nodes.empty());
}

} // namespace

int main()
{
    isolation();
    namesAndFailures();
    return ragtime_test::finish();
}
