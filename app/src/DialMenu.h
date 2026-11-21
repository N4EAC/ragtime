#ifndef RAGTIME_DIAL_MENU_H
#define RAGTIME_DIAL_MENU_H

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <wx/datetime.h>
#include <wx/event.h>
#include <wx/longlong.h>

#include "ragtime/radio/HotFrequencies.h"

class wxButton;
class wxMouseEvent;

// The dial's dropdown beside the ear (Eduardo N4EAC's idea, Jeff
// 2026-11-20): a click lists the operator's hot frequencies and tunes the
// rig to the one picked; a right-click offers Edit... (the list's file, in
// the default graphical text editor on macOS) and Custom... (a frequency typed); Ctrl with the
// mouse wheel steps the rig 1 kHz, Shift 10 kHz. Tuning needs the rig
// through rigctld or Hamlib, which is when the dial shows at all.
//
// The list is a text file (hotFrequenciesPath(), radio::parseHotFrequencies),
// written with Jeff's seed list the first time it is edited and read again
// whenever it changes. Until then the seed list is shown.
class DialMenu : public wxEvtHandler
{
public:
    struct Rig
    {
        std::function<std::optional<uint64_t>()> dialHz;
        std::function<bool()> canTune;
        // Tune to hz; how is for the flight recorder: "list", "custom",
        // "wheel".
        std::function<void(uint64_t hz, const std::string& how)> tune;
    };

    DialMenu(wxButton* button, std::string path, Rig rig);
    ~DialMenu() override;

    // Looks at the file now and then, reading it again when it changed.
    // The window's clock calls it.
    void poll();
    // While a tune hasn't been read back yet (a few seconds at most): the
    // frequency asked for, to show in place of the dial.
    std::optional<uint64_t> pending(std::optional<uint64_t> dial);
    // Lines for the dial's tooltip: how to use it, and lines of the file
    // skipped.
    std::string tip() const;
    const std::vector<ragtime::radio::HotFrequency>& entries() const { return list_.entries; }

private:
    void onClick(wxCommandEvent& event);
    void onContext(wxContextMenuEvent& event);
    void onWheel(wxMouseEvent& event);
    // The same, from the bar under the dial (where GTK sends it).
    void onBarWheel(wxMouseEvent& event);
    // full: the list, then Edit... and Custom...; else only those two.
    void showMenu(bool full);
    void tuneTo(uint64_t hz, const std::string& how);
    void custom();
    void edit();
    // The file, with the seed list in it if it wasn't there.
    bool ensureFile(std::string& why);
    void load(bool force);
    // Starts launches_[from] or the first after it whose program is here.
    bool launch(size_t from);

    wxButton* button_;
    std::string path_;
    Rig rig_;
    ragtime::radio::HotFrequencyList list_;
    bool fromFile_ = false;
    wxDateTime seenTime_;
    wxULongLong seenSize_ = 0;
    std::string readProblem_;
    uint64_t lookedMs_ = 0;
    std::optional<uint64_t> target_;
    uint64_t targetMs_ = 0;
    int wheelRest_ = 0;
    std::vector<ragtime::radio::EditorLaunch> launches_;
    // Cleared when this goes, for an osascript ending after.
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

#endif
