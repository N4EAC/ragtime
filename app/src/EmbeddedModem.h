#ifndef RAGTIME_EMBEDDED_MODEM_H
#define RAGTIME_EMBEDDED_MODEM_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <wx/timer.h>

namespace data2g::app { class Station; }
namespace ragtime::audio { class EarAudio; }
struct StationPrefs;

// The native Data2G station runs in Ragtime's process. It owns the modem
// threads and speaks the same loopback protocols as a standalone station.
class EmbeddedModem
{
public:
    using Failure = std::function<void(const std::string&)>;

    explicit EmbeddedModem(Failure onFailure);
    ~EmbeddedModem();
    std::string start(const StationPrefs& prefs);
    void stop();
    void readEarAudio(ragtime::audio::EarAudio& ear);

    static int runWithRuntime(int argc, char** argv, int (*run)(int, char**));
    static std::vector<std::string> devices(bool input);

private:
    std::unique_ptr<data2g::app::Station> station_;
    wxTimer timer_;
    Failure onFailure_;
    std::uint64_t earSamples_ = 0;
};

#endif
