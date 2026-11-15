#ifndef RAGTIME_EMBEDDED_MODEM_H
#define RAGTIME_EMBEDDED_MODEM_H

#include <complex>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <wx/timer.h>

namespace data2g::app { class Station; }
namespace ragtime::audio { class EarAudio; }
struct StationPrefs;

struct ModemConstellation
{
    std::string mode;
    bool tones;
    bool live;
    bool lost;
    std::vector<std::complex<float>> symbols;
    std::vector<std::complex<float>> points;
};

struct ModemTelemetry
{
    std::string audio;
    std::string link;
    std::string mode;
    bool busy = false;
    bool ptt = false;
    std::int64_t rxBytes = 0, txBytes = 0;
    std::uint64_t overflows = 0, underruns = 0, late = 0, dropped = 0, decodeDropped = 0;
    double backlogSeconds = 0.0;
};

struct ModemBurst
{
    std::string mode;
    bool lost;
    std::optional<double> snrDb;
};

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
    std::optional<ModemConstellation> takeConstellation();
    std::optional<ModemTelemetry> telemetry() const;
    std::optional<ModemBurst> takeLatestBurst();

    static int runWithRuntime(int argc, char** argv, int (*run)(int, char**));
    static std::vector<std::string> devices(bool input);

private:
    std::unique_ptr<data2g::app::Station> station_;
    wxTimer timer_;
    Failure onFailure_;
    std::uint64_t earSamples_ = 0;
};

#endif
