#include "FrequencySelector.h"

#include <algorithm>
#include <sstream>
#include <utility>

#include <wx/file.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/log.h>

#include "HostConnection.h"
#include "Prefs.h"

namespace
{
constexpr const char* LAST_FREQUENCY = "/Radio/LastSelectedFrequencyMHz";
constexpr const char* DEFAULT_LIST =
    "# Radio dial frequencies in MHz, one per line. Save this file to refresh the list.\n"
    "3.566\n7.066\n7.110\n14.066\n28.066\n";

std::optional<uint64_t> parseMHz(std::string value)
{
    value = value.substr(0, value.find('#'));
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) return std::nullopt;
    const auto last = value.find_last_not_of(" \t\r");
    value = value.substr(first, last - first + 1);
    if (value.find_first_not_of("0123456789.") != std::string::npos) return std::nullopt;
    const auto dot = value.find('.');
    if (value.size() > 13 || (dot == std::string::npos && value.size() > 6) || dot == 0 ||
        (dot != std::string::npos && dot > 6) || (dot != std::string::npos &&
        (value.find('.', dot + 1) != std::string::npos || value.size() - dot - 1 > 6))) return std::nullopt;
    std::string digits = value;
    int decimals = 0;
    if (dot != std::string::npos)
    {
        decimals = int(value.size() - dot - 1);
        digits.erase(dot, 1);
    }
    if (digits.empty()) return std::nullopt;
    uint64_t hz = 0;
    for (const char digit : digits) hz = hz * 10 + uint64_t(digit - '0');
    for (int i = decimals; i < 6; ++i) hz *= 10;
    if (hz < 1000 || hz >= 1000000000000ULL) return std::nullopt;
    return hz;
}

wxString mhz(uint64_t hz)
{
    // Integer formatting is independent of the user's decimal locale.
    auto fraction = wxString::Format("%06u", unsigned(hz % 1000000));
    while (fraction.size() > 3 && fraction.Last() == '0') fraction.RemoveLast();
    return wxString::Format("%llu.", static_cast<unsigned long long>(hz / 1000000)) + fraction;
}
} // namespace

FrequencySelector::FrequencySelector(wxWindow* parent)
    : wxChoice(parent, wxID_ANY)
    , timer_(this)
{
    SetName("Radio frequency");
    SetMinSize(FromDIP(wxSize(145, -1)));
    remembered_ = parseMHz(loadString(LAST_FREQUENCY, "7.066")).value_or(7066000);
    const auto path = wxString::FromUTF8(frequencyListPath());
    wxFileName::Mkdir(wxFileName(path).GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    if (!wxFileName::FileExists(path))
    {
        wxFile file;
        if (file.Create(path, false)) file.Write(DEFAULT_LIST);
    }
    reload();
    Disable();
    Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        const int index = GetSelection();
        if (index == wxNOT_FOUND || size_t(index) >= frequencies_.size()) return;
        if (!hooks_.available || !hooks_.available() || !hooks_.tune(frequencies_[size_t(index)]))
        {
            selectRemembered();
            wxMessageBox("The radio must be connected and receiving before changing frequency.",
                         "Radio frequency", wxOK | wxICON_INFORMATION, this);
            return;
        }
        pending_ = frequencies_[size_t(index)];
        startedMs_ = HostConnection::nowMs();
        tick();
    });
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { tick(); }, timer_.GetId());
}

FrequencySelector::~FrequencySelector()
{
    timer_.Stop();
}

void FrequencySelector::setHooks(Hooks hooks)
{
    hooks_ = std::move(hooks);
    tick();
    timer_.Start(500);
}

void FrequencySelector::cancel()
{
    timer_.Stop();
    reset();
}

void FrequencySelector::reset()
{
    if (pending_ && hooks_.cancel) hooks_.cancel();
    pending_ = 0;
    selectRemembered();
}

void FrequencySelector::selectRemembered()
{
    const auto selected = std::find(frequencies_.begin(), frequencies_.end(), remembered_);
    SetSelection(selected == frequencies_.end() ? wxNOT_FOUND : int(selected - frequencies_.begin()));
}

void FrequencySelector::reload()
{
    if (pending_) return;
    wxLogNull silent;
    wxFile file(wxString::FromUTF8(frequencyListPath()));
    wxString text;
    if (!file.IsOpened() || file.Length() > 65536 || !file.ReadAll(&text))
    {
        fileProblem_ = "Could not read the frequency list (maximum 64 KB).";
        return;
    }
    const auto bytes = std::string(text.utf8_str());
    if (!frequencies_.empty() && bytes == fileText_)
    {
        fileProblem_.clear();
        return;
    }
    std::vector<uint64_t> frequencies;
    std::istringstream input(bytes);
    int lineNumber = 0;
    for (std::string line; std::getline(input, line);)
    {
        ++lineNumber;
        const auto content = line.substr(0, line.find('#'));
        if (content.find_first_not_of(" \t\r") == std::string::npos) continue;
        const auto hz = parseMHz(line);
        if (!hz)
        {
            fileProblem_ = wxString::Format("Invalid MHz frequency on line %d.", lineNumber);
            return;
        }
        if (std::find(frequencies.begin(), frequencies.end(), *hz) == frequencies.end()) frequencies.push_back(*hz);
    }
    if (frequencies.empty())
    {
        fileProblem_ = "The frequency list must contain at least one frequency in MHz.";
        return;
    }
    // Preserve the last-used frequency when it is removed from the editable list.
    if (std::find(frequencies.begin(), frequencies.end(), remembered_) == frequencies.end())
        frequencies.push_back(remembered_);
    frequencies_ = std::move(frequencies);
    fileText_ = bytes;
    fileProblem_.clear();
    Clear();
    for (const auto hz : frequencies_) Append(mhz(hz) + " MHz");
    selectRemembered();
}

void FrequencySelector::finish(const wxString& problem)
{
    if (hooks_.cancel) hooks_.cancel();
    pending_ = 0;
    selectRemembered();
    if (!problem.empty()) wxMessageBox(problem, "Radio frequency", wxOK | wxICON_ERROR, this);
}

void FrequencySelector::tick()
{
    const bool available = hooks_.available && hooks_.available();
    if (pending_)
    {
        const auto problem = hooks_.problem();
        if (!available)
            finish("Frequency change cancelled because the radio disconnected or started transmitting.");
        else if (!problem.empty())
            finish(wxString::FromUTF8(problem));
        else if (hooks_.read() == pending_)
        {
            remembered_ = pending_;
            saveString(LAST_FREQUENCY, std::string(mhz(remembered_).utf8_str()));
            finish({});
        }
        else if (HostConnection::nowMs() - startedMs_ >= 10000)
            finish("The radio did not confirm the selected frequency within 10 seconds.");
    }
    reload();
    Enable(available && !pending_ && !frequencies_.empty());
    wxString tip = pending_ ? "Waiting for the radio to confirm the selected frequency." :
                              "Select a frequency to tune the connected radio. Values are in MHz.";
    if (!available) tip += "\nConnect the radio under Settings > Rig and stop transmitting to tune.";
    if (!fileProblem_.empty()) tip += "\n" + fileProblem_;
    tip += "\nEdit the list under File > Edit frequency list. Changes load automatically.";
    SetToolTip(tip);
}
