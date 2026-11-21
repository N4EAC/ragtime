#include "SottoPaths.h"

#include <cstdint>
#include <filesystem>
#include <system_error>
#include <mach-o/dyld.h>

namespace ragtime
{
namespace voice
{

std::vector<std::string> macosSottoCandidates()
{
    std::vector<std::string> out;
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (size != 0 && _NSGetExecutablePath(buffer.data(), &size) == 0)
    {
        // dyld may return a symlink or a path containing '..'. Resolve it
        // before finding the bundle, so launching through a symlink works.
        std::error_code error;
        auto program = std::filesystem::canonical(buffer.data(), error);
        if (error) program = std::filesystem::path(buffer.data());
        const auto folder = program.parent_path();
        std::vector<std::filesystem::path> folders;
        const bool bundled = folder.filename() == "MacOS" && folder.parent_path().filename() == "Contents";
        if (bundled) folders.push_back(folder.parent_path() / "Frameworks");
        folders.push_back(folder);
        // Development builds keep the codec output beside app/ and tests/.
        auto ancestor = folder;
        for (int i = 0; !bundled && i < 6 && ancestor.has_parent_path(); ++i)
        {
            folders.push_back(ancestor / "build-sotto" / "out");
            const auto parent = ancestor.parent_path();
            if (parent == ancestor) break;
            ancestor = parent;
        }
        for (const auto& candidate : folders)
        {
            for (const char* name : {"libsotto.0.dylib", "libsotto.dylib"})
                out.push_back((candidate / name).string());
        }
    }
    out.push_back("libsotto.0.dylib");
    out.push_back("libsotto.dylib");
    return out;
}

} // namespace voice
} // namespace ragtime
