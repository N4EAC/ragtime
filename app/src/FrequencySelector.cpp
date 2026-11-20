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

std::optional<uint64_t> parseNumber(std::string value, unsigned places)
{
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) return std::nullopt;
    value = value.substr(first, value.find_last_not_of(" \t\r") - first + 1);
    if (value.size() > 18 || value.find_first_not_of("0123456789.") != std::string::npos)
        return std::nullopt;
    const auto dot = value.find('.');
    if (dot == 0 || (dot != std::string::npos &&
        (value.find('.', dot + 1) != std::string::npos || value.size() - dot - 1 > places)))
        return std::nullopt;
    unsigned decimals = 0;
    if (dot != std::string::npos)
    {
        decimals = unsigned(value.size() - dot - 1);
        value.erase(dot, 1);
    }
    uint64_t hz = 0;
    for (const char digit : value)
    {
        hz = hz * 10 + uint64_t(digit - '0');
        if (hz >= 1000000000000ULL) return std::nullopt;
    }
    for (unsigned i = decimals; i < places; ++i)
    {
        hz *= 10;
        if (hz >= 1000000000000ULL) return std::nullopt;
    }
    return hz >= 1000 ? std::optional<uint64_t>(hz) : std::nullopt;
}

std::optional<uint64_t> parseMHz(std::string value)
{
    return parseNumber(value.substr(0, value.find('#')), 6);
}

std::optional<uint64_t> parseEntry(wxString text)
{
    text.Trim().Trim(false);
    unsigned places = 3; // An unlabelled number is kHz: 7100 = 7.100 MHz.
    const auto lower = text.Lower();
    if (lower.EndsWith("mhz")) { places = 6; text.RemoveLast(3); }
    else if (lower.EndsWith("khz")) { text.RemoveLast(3); }
    else if (lower.EndsWith("hz")) { places = 0; text.RemoveLast(2); }
    return parseNumber(std::string(text.utf8_str()), places);
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
    : wxComboBox(parent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 0, nullptr,
                 wxCB_DROPDOWN | wxTE_PROCESS_ENTER)
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
    Bind(wxEVT_TEXT, [this](wxCommandEvent&) { if (!updating_) dirty_ = true; });
    Bind(wxEVT_COMBOBOX, [this](wxCommandEvent&) {
        const int index = GetSelection();
        if (index != wxNOT_FOUND && size_t(index) < frequencies_.size()) submit(frequencies_[size_t(index)]);
    });
    Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) {
        const auto hz = parseEntry(GetValue());
        if (!hz)
        {
            wxMessageBox("Enter a frequency in kHz, such as 7100 for 7.100 MHz. "
                         "Explicit units also work: 7.100 MHz or 7100000 Hz.",
                         "Radio frequency", wxOK | wxICON_INFORMATION, this);
            return;
        }
        submit(*hz);
    });
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& e) {
        if (e.GetKeyCode() == WXK_ESCAPE)
        {
            dirty_ = false;
            selectRemembered();
        }
        else e.Skip();
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
    dirty_ = false;
    selectRemembered();
}

bool FrequencySelector::editing() const
{
    auto* focus = wxWindow::FindFocus();
    return dirty_ || HasFocus() || (focus && IsDescendant(focus));
}

void FrequencySelector::submit(uint64_t hz)
{
    if (pending_) return;
    if (!hooks_.available || !hooks_.available() || !hooks_.tune || !hooks_.tune(hz))
    {
        wxMessageBox("The radio must be connected and receiving before changing frequency.",
                     "Radio frequency", wxOK | wxICON_INFORMATION, this);
        return;
    }
    dirty_ = false;
    pending_ = hz;
    ChangeValue(mhz(hz) + " MHz");
    startedMs_ = HostConnection::nowMs();
    tick();
}

void FrequencySelector::selectRemembered()
{
    const auto actual = hooks_.read ? hooks_.read() : std::nullopt;
    ChangeValue(mhz(actual.value_or(remembered_)) + " MHz");
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
    const bool keepEntry = editing();
    const auto entry = GetValue();
    updating_ = true;
    Clear();
    for (const auto hz : frequencies_) Append(mhz(hz) + " MHz");
    if (keepEntry) ChangeValue(entry);
    else selectRemembered();
    updating_ = false;
}

void FrequencySelector::finish(const wxString& problem)
{
    if (hooks_.cancel) hooks_.cancel();
    pending_ = 0;
    dirty_ = false;
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
    if (!pending_ && !editing()) selectRemembered();
    Enable(available && !pending_);
    wxString tip = pending_ ? "Waiting for the radio to confirm the selected frequency." :
                              "Type kHz (7100 = 7.100 MHz), or an explicit MHz/Hz value, and press Enter to tune.\n"
                              "Choose a preset to tune immediately. Escape restores the radio reading.";
    if (!available) tip += "\nConnect the radio under Settings > Rig and stop transmitting to tune.";
    if (!fileProblem_.empty()) tip += "\n" + fileProblem_;
    tip += "\nEdit the list under File > Edit frequency list. Changes load automatically.";
    SetToolTip(tip);
}
