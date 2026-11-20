#include "CoreAudioDevices.h"

#include <algorithm>
#include <map>
#include <set>

namespace ragtime
{
namespace audio
{
namespace macos
{

std::string deviceName(const std::string& uid, bool input)
{
    std::string name = input ? "coreaudio:input:" : "coreaudio:output:";
    // Device names are persisted as a semicolon-separated list by voice.
    for (unsigned char c : uid)
    {
        if (c == '%' || c == ';')
        {
            const char* hex = "0123456789ABCDEF";
            name += '%';
            name += hex[c >> 4];
            name += hex[c & 15];
        }
        else name += static_cast<char>(c);
    }
    return name;
}

std::string defaultName(const DeviceSnapshot& s, bool input)
{
    const auto id = input ? s.defaultInput : s.defaultOutput;
    for (const auto& d : s.devices)
        if (d.id == id) return deviceName(d.uid, input);
    return {};
}

const Device* findDevice(const DeviceSnapshot& s, const std::string& name, bool input)
{
    for (const auto& d : s.devices)
        if (deviceName(d.uid, input) == name) return &d;
    return nullptr;
}

ear::Graph deviceGraph(const DeviceSnapshot& s)
{
    ear::Graph graph;
    if (!s.problem.empty()) return graph;
    std::map<uint32_t, uint32_t> groups;
    for (const auto& d : s.devices) groups[d.id] = d.id;
    const auto root = [&](uint32_t id) {
        while (groups.at(id) != id) id = groups.at(id);
        return id;
    };
    for (const auto& d : s.devices)
        for (auto other : d.related)
            if (groups.count(other)) groups[root(other)] = root(d.id);

    std::map<uint32_t, uint32_t> inputs, outputs;
    uint32_t next = 1;
    for (const auto& d : s.devices)
    {
        for (bool input : {true, false})
        {
            if (!(input ? d.inputs : d.outputs)) continue;
            ear::GraphNode n;
            n.id = next++;
            n.name = deviceName(d.uid, input);
            n.description = d.label;
            n.mediaClass = input ? "Audio/Source" : "Audio/Sink";
            n.device = "coreaudio:card:" + std::to_string(root(d.id));
            (input ? inputs : outputs)[d.id] = n.id;
            graph.nodes.push_back(std::move(n));
        }
    }
    for (const auto& p : s.processes)
    {
        if (!ear::isData2GProgram(p.program)) continue;
        const auto client = next++;
        graph.clients.push_back({client, p.program, p.pid});
        for (bool input : {true, false})
        {
            for (auto id : input ? p.inputs : p.outputs)
            {
                const auto& nodes = input ? inputs : outputs;
                const auto found = nodes.find(id);
                if (found == nodes.end()) return {}; // Unresolved host device: offer nothing.
                ear::GraphNode n;
                n.id = next++;
                n.clientId = client;
                n.appName = p.program;
                n.mediaClass = input ? "Stream/Input/Audio" : "Stream/Output/Audio";
                graph.links.push_back(input ? ear::GraphLink{found->second, n.id}
                                            : ear::GraphLink{n.id, found->second});
                graph.nodes.push_back(std::move(n));
            }
        }
    }
    return graph;
}

std::vector<std::string> rememberRelated(const ear::Graph& graph, const std::vector<std::string>& remembered)
{
    std::set<std::string> cards;
    for (const auto& n : graph.nodes)
        if (!n.device.empty() && std::find(remembered.begin(), remembered.end(), n.name) != remembered.end())
            cards.insert(n.device);
    auto out = remembered;
    for (const auto& n : graph.nodes)
        if (!n.device.empty() && cards.count(n.device) && std::find(out.begin(), out.end(), n.name) == out.end())
            out.push_back(n.name);
    return out;
}

} // namespace macos
} // namespace audio
} // namespace ragtime
