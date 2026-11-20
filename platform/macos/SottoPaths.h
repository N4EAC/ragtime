#ifndef RAGTIME_MACOS_SOTTO_PATHS_H
#define RAGTIME_MACOS_SOTTO_PATHS_H

#include <string>
#include <vector>

namespace ragtime
{
namespace voice
{

// Bundle Frameworks first, then beside the executable, then dyld's search.
// RAGTIME_SOTTO is handled by the common loader before these candidates.
std::vector<std::string> macosSottoCandidates();

} // namespace voice
} // namespace ragtime

#endif
