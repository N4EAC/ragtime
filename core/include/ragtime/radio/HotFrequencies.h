// Hot frequencies: the operator's own short list of dial frequencies,
// picked from the dial's dropdown beside the ear to tune the rig (Eduardo
// N4EAC's idea, 2026-11-20). They live in a plain text file the operator
// edits: one frequency in MHz per line, "#" starting a comment, and a
// comment after a frequency shown beside it in the list
// ("5.36 # 9.15 watts ERP Maximum"). Tuning needs the rig through rigctld
// or Hamlib; the list only says where to go.
//
// Also here: the frequency typed for Custom..., the mouse wheel's steps,
// and the ways to open the file in the operator's $EDITOR in a terminal on
// each desktop. Nothing here touches a file or starts a program.
#ifndef RAGTIME_RADIO_HOT_FREQUENCIES_H
#define RAGTIME_RADIO_HOT_FREQUENCIES_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ragtime
{
namespace radio
{

struct HotFrequency
{
    uint64_t hz = 0;
    std::string note;   // the comment after it, trimmed; may be empty

    bool operator==(const HotFrequency& o) const { return hz == o.hz && note == o.note; }
};

struct HotFrequencyList
{
    std::vector<HotFrequency> entries;   // in the file's order
    // Lines skipped, for the dial's tooltip: "line 7: \"14.07.4\" is no
    // frequency in MHz".
    std::vector<std::string> problems;
};

// What a new file starts with: Jeff's header and list (2026-11-20).
extern const char* const HOT_FREQUENCIES_SEED;

// Reads the file's text. Blank lines and comments are skipped; so is a
// line that isn't a frequency, with a problem saying so.
HotFrequencyList parseHotFrequencies(const std::string& text);

// A frequency typed by the operator: MHz unless it says otherwise, "7.066",
// "7.066 MHz", "7066 kHz", "7066000 Hz", with a comma taken as a decimal
// point. Nothing, or anything outside 10 kHz to 10 GHz, is no frequency.
std::optional<uint64_t> parseFrequency(const std::string& text);

// The mouse wheel over the dial: Ctrl steps 1 kHz (0.001 MHz), Shift
// 10 kHz (0.01 MHz), a notch at a time, up or down; never below 10 kHz.
constexpr uint64_t WHEEL_STEP_HZ = 1000;
constexpr uint64_t WHEEL_SHIFT_STEP_HZ = 10000;
uint64_t stepFrequency(uint64_t hz, int notches, uint64_t stepHz);

// Opening the file in the operator's editor.
enum class Desktop
{
    Linux,      // and the other Unixes with X11 or Wayland
    Mac,
    Windows,
};

struct EditorLaunch
{
    // The program that must be found on the PATH for this way to be tried
    // (empty: always there).
    std::string program;
    // What to run: argv, or on Windows a command line (commandLine set).
    std::vector<std::string> argv;
    std::string commandLine;
    // Windows: the child needs a console window of its own.
    bool console = false;
    // Try the next way if this one ends in failure (osascript refused).
    bool nextIfFails = false;
};

// The editor's words from $EDITOR ("emacs -nw" is two), split at spaces;
// with none, nano when there is one, else vi.
std::vector<std::string> editorWords(const std::string& editorEnv, bool haveNano);

// The ways to open path for editing, best first; the caller runs the first
// whose program it finds.
//  - Linux: $EDITOR in a terminal: $TERMINAL, then the desktop's own
//    ($XDG_CURRENT_DESKTOP: Konsole on KDE, and so on), then
//    x-terminal-emulator, GNOME Terminal, Ptyxis, Console, Konsole, Xfce,
//    MATE, kitty, Alacritty, foot, WezTerm and xterm; failing every one,
//    xdg-open (the desktop's text editor).
//  - Mac: the default graphical text editor through open -t. Shell editor
//    and terminal preferences do not change this desktop action.
//  - Windows: with $EDITOR set, it in a console window; else Notepad.
std::vector<EditorLaunch> editorLaunches(Desktop desktop, const std::string& path, const std::string& editorEnv,
                                         const std::string& terminalEnv, const std::string& currentDesktop,
                                         bool haveNano);

} // namespace radio
} // namespace ragtime

#endif
