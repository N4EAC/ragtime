// Hot frequencies: the file's lines (Jeff's seed, comments, notes, lines
// that aren't frequencies), a typed frequency to the hertz, the mouse
// wheel's steps, and the ways to open the file in an editor.
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Check.h"
#include "ragtime/radio/HotFrequencies.h"

using namespace ragtime::radio;

namespace
{

void seed()
{
    HotFrequencyList list = parseHotFrequencies(HOT_FREQUENCIES_SEED);
    CHECK(list.problems.empty());
    const std::vector<uint64_t> want = {1806000,  3566000,  5346500,  5360000,  5403500,  7066000,
                                        7110000,  10146000, 14066000, 14110000, 18106000, 28066000};
    CHECK_EQ(list.entries.size(), want.size());
    for (size_t i = 0; i < want.size() && i < list.entries.size(); i++) CHECK_EQ(list.entries[i].hz, want[i]);
    CHECK_EQ(list.entries[3].note, std::string("9.15 watts ERP Maximum"));
    CHECK(list.entries[0].note.empty());
    const std::string header = "# User-definable frequencies\n# Enter one frequency per line\n";
    CHECK_EQ(std::string(HOT_FREQUENCIES_SEED).substr(0, header.size()), header);
}

void lines()
{
    HotFrequencyList list = parseHotFrequencies("  # just a comment\r\n"
                                                "\n"
                                                "14.0745\t# FT4 neighbour  \r\n"
                                                "14.07.4\n"
                                                "hello # note\n"
                                                "  7,066  \n"
                                                "#7.1\n"
                                                "3.5735"); // no newline at the end
    CHECK_EQ(list.entries.size(), (size_t)3);
    CHECK_EQ(list.entries[0].hz, (uint64_t)14074500);
    CHECK_EQ(list.entries[0].note, std::string("FT4 neighbour"));
    CHECK_EQ(list.entries[1].hz, (uint64_t)7066000);
    CHECK_EQ(list.entries[2].hz, (uint64_t)3573500);
    CHECK_EQ(list.problems.size(), (size_t)2);
    CHECK_EQ(list.problems[0], std::string("line 4: \"14.07.4\" is no frequency in MHz"));
    CHECK_EQ(list.problems[1], std::string("line 5: \"hello\" is no frequency in MHz"));
    CHECK(parseHotFrequencies("").entries.empty());
    CHECK(parseHotFrequencies("").problems.empty());
}

void typed()
{
    CHECK(parseFrequency("7.066") == std::optional<uint64_t>(7066000));
    CHECK(parseFrequency(" 5.3465 MHz ") == std::optional<uint64_t>(5346500));
    CHECK(parseFrequency("5.3465mhz") == std::optional<uint64_t>(5346500));
    CHECK(parseFrequency("7066 kHz") == std::optional<uint64_t>(7066000));
    CHECK(parseFrequency("7066.5kHz") == std::optional<uint64_t>(7066500));
    CHECK(parseFrequency("7066000 Hz") == std::optional<uint64_t>(7066000));
    CHECK(parseFrequency("14,074") == std::optional<uint64_t>(14074000));
    CHECK(parseFrequency(".5") == std::optional<uint64_t>(500000));
    CHECK(parseFrequency("7.") == std::optional<uint64_t>(7000000));
    // Past the hertz: rounded.
    CHECK(parseFrequency("7.0660005") == std::optional<uint64_t>(7066001));
    CHECK(parseFrequency("7.0660004") == std::optional<uint64_t>(7066000));
    CHECK(!parseFrequency(""));
    CHECK(!parseFrequency("MHz"));
    CHECK(!parseFrequency("."));
    CHECK(!parseFrequency("-7.066"));
    CHECK(!parseFrequency("7.066.1"));
    CHECK(!parseFrequency("7 066"));
    CHECK(!parseFrequency("0.001"));          // 1 kHz: no dial
    CHECK(!parseFrequency("20000"));          // 20 GHz
    CHECK(!parseFrequency("99999999999999999999999"));
}

void wheel()
{
    CHECK_EQ(stepFrequency(7066000, 1, WHEEL_STEP_HZ), (uint64_t)7067000);
    CHECK_EQ(stepFrequency(7066000, -3, WHEEL_STEP_HZ), (uint64_t)7063000);
    CHECK_EQ(stepFrequency(7066000, 2, WHEEL_SHIFT_STEP_HZ), (uint64_t)7086000);
    CHECK_EQ(stepFrequency(7066000, -1, WHEEL_SHIFT_STEP_HZ), (uint64_t)7056000);
    CHECK_EQ(stepFrequency(7066000, 0, WHEEL_STEP_HZ), (uint64_t)7066000);
    CHECK_EQ(stepFrequency(15000, -1, WHEEL_SHIFT_STEP_HZ), (uint64_t)10000);
}

std::vector<std::string> programs(const std::vector<EditorLaunch>& ls)
{
    std::vector<std::string> out;
    for (const EditorLaunch& l : ls) out.push_back(l.program);
    return out;
}

void editors()
{
    CHECK(editorWords("", true) == std::vector<std::string>{"nano"});
    CHECK(editorWords("  ", false) == std::vector<std::string>{"vi"});
    CHECK(editorWords("emacs -nw", true) == (std::vector<std::string>{"emacs", "-nw"}));

    const std::string path = "/home/op/.local/share/ragtime/frequencies.txt";
    // Linux, nothing set: x-terminal-emulator, then the known terminals,
    // xterm last but for xdg-open.
    std::vector<EditorLaunch> ls = editorLaunches(Desktop::Linux, path, "", "", "", true);
    std::vector<std::string> p = programs(ls);
    CHECK_EQ(p.front(), std::string("x-terminal-emulator"));
    CHECK_EQ(p[p.size() - 2], std::string("xterm"));
    CHECK_EQ(p.back(), std::string("xdg-open"));
    CHECK(ls.front().argv == (std::vector<std::string>{"x-terminal-emulator", "-e", "nano", path}));
    CHECK(ls[1].argv == (std::vector<std::string>{"gnome-terminal", "--", "nano", path}));
    CHECK(ls.back().argv == (std::vector<std::string>{"xdg-open", path}));
    // Each program once.
    for (size_t i = 0; i < p.size(); i++)
        for (size_t j = i + 1; j < p.size(); j++) CHECK(p[i] != p[j]);

    // KDE's own first, with $EDITOR's words.
    ls = editorLaunches(Desktop::Linux, path, "vim -p", "", "KDE", true);
    CHECK(ls.front().argv == (std::vector<std::string>{"konsole", "-e", "vim", "-p", path}));
    // $TERMINAL ahead of all: a known one with its own words, else -e.
    ls = editorLaunches(Desktop::Linux, path, "", "kitty", "ubuntu:GNOME", false);
    CHECK(ls[0].argv == (std::vector<std::string>{"kitty", "vi", path}));
    CHECK_EQ(ls[1].program, std::string("gnome-terminal"));
    ls = editorLaunches(Desktop::Linux, path, "", "st", "", true);
    CHECK(ls[0].argv == (std::vector<std::string>{"st", "-e", "nano", path}));

    // Mac: Terminal through osascript, the path quoted for the shell and
    // the whole for AppleScript; then TextEdit.
    ls = editorLaunches(Desktop::Mac, "/Users/op/It's \"here\"/f.txt", "", "", "", false);
    CHECK_EQ(ls.size(), (size_t)2);
    CHECK_EQ(ls[0].program, std::string("osascript"));
    CHECK(ls[0].nextIfFails);
    CHECK_EQ(ls[0].argv[6], std::string("do script \"${EDITOR:-nano} '/Users/op/It'\\\\''s \\\"here\\\"/f.txt'; exit\""));
    CHECK(ls[1].argv == (std::vector<std::string>{"open", "-t", "/Users/op/It's \"here\"/f.txt"}));

    // Windows: $EDITOR in a console, else Notepad.
    ls = editorLaunches(Desktop::Windows, "C:\\Users\\op\\f.txt", "", "", "", false);
    CHECK_EQ(ls.size(), (size_t)1);
    CHECK_EQ(ls[0].commandLine, std::string("notepad.exe \"C:\\Users\\op\\f.txt\""));
    CHECK(!ls[0].console);
    ls = editorLaunches(Desktop::Windows, "C:\\f.txt", "vim", "", "", false);
    CHECK_EQ(ls.size(), (size_t)2);
    CHECK_EQ(ls[0].commandLine, std::string("cmd.exe /c vim \"C:\\f.txt\""));
    CHECK(ls[0].console);
}

} // namespace

int main()
{
    seed();
    lines();
    typed();
    wheel();
    editors();
    return ragtime_test::finish();
}
