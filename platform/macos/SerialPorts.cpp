#include "ragtime/rig/HamlibRig.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace ragtime
{
namespace rig
{

std::vector<std::string> serialPorts()
{
    // Call-out devices avoid waiting for carrier detect. Do not offer the
    // matching tty.* device as a second choice for the same interface.
    std::vector<std::string> ports;
    std::error_code error;
    std::filesystem::directory_iterator entry("/dev", error), end;
    while (!error && entry != end)
    {
        const auto& path = entry->path();
        if (path.filename().string().compare(0, 3, "cu.") == 0)
            ports.push_back(path.string());
        entry.increment(error);
    }
    std::sort(ports.begin(), ports.end());
    return ports;
}

} // namespace rig
} // namespace ragtime
