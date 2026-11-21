#include "ragtime/radio/HotFrequencies.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ragtime
{
namespace radio
{

const char* const HOT_FREQUENCIES_SEED = "# User-definable frequencies\n"
                                         "# Enter one frequency per line\n"
                                         "1.806\n"
                                         "3.566\n"
                                         "5.3465\n"
                                         "5.36 # 9.15 watts ERP Maximum\n"
                                         "5.4035\n"
                                         "7.066\n"
                                         "7.110\n"
                                         "10.146\n"
                                         "14.066\n"
                                         "14.110\n"
                                         "18.106\n"
                                         "28.066\n";

namespace
{

constexpr uint64_t LOWEST_HZ = 10000;
constexpr uint64_t HIGHEST_HZ = 10000000000ULL;

std::string trim(const std::string& s)
{
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

std::string lower(std::string s)
{
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// "5.3465" times 10^digits, exactly, rounded at the next digit: no
// floating point, so 5.3465 MHz is 5346500 Hz to the hertz (60 m's
// channels want the dial exact).
std::optional<uint64_t> decimal(const std::string& s, int digits)
{
    if (s.empty()) return std::nullopt;
    uint64_t whole = 0;
    size_t i = 0;
    size_t seen = 0;
    for (; i < s.size() && std::isdigit((unsigned char)s[i]); i++, seen++)
    {
        if (whole > HIGHEST_HZ) return std::nullopt;
        whole = whole * 10 + (uint64_t)(s[i] - '0');
    }
    uint64_t frac = 0;
    int got = 0;
    bool roundUp = false;
    if (i < s.size() && (s[i] == '.' || s[i] == ','))
    {
        for (i++; i < s.size() && std::isdigit((unsigned char)s[i]); i++, seen++)
        {
            if (got < digits)
            {
                frac = frac * 10 + (uint64_t)(s[i] - '0');
                got++;
            }
            else if (got == digits)
            {
                roundUp = s[i] >= '5';
                got++;
            }
        }
    }
    if (i != s.size() || seen == 0) return std::nullopt;
    for (int k = std::min(got, digits); k < digits; k++) frac *= 10;
    uint64_t scale = 1;
    for (int k = 0; k < digits; k++) scale *= 10;
    if (whole > HIGHEST_HZ) return std::nullopt;
    return whole * scale + frac + (roundUp ? 1 : 0);
}

} // namespace

std::optional<uint64_t> parseFrequency(const std::string& text)
{
    std::string s = lower(trim(text));
    int digits = 6;   // MHz
    auto unit = [&s](const char* name) {
        const std::string u(name);
        if (s.size() < u.size() || s.compare(s.size() - u.size(), u.size(), u) != 0) return false;
        s = trim(s.substr(0, s.size() - u.size()));
        return true;
    };
    if (unit("mhz"))
        digits = 6;
    else if (unit("khz"))
        digits = 3;
    else if (unit("hz"))
        digits = 0;
    std::optional<uint64_t> hz = decimal(s, digits);
    if (!hz || *hz < LOWEST_HZ || *hz > HIGHEST_HZ) return std::nullopt;
    return hz;
}

HotFrequencyList parseHotFrequencies(const std::string& text)
{
    HotFrequencyList list;
    size_t at = 0;
    int number = 0;
    while (at <= text.size())
    {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        number++;
        std::string note;
        if (size_t hash = line.find('#'); hash != std::string::npos)
        {
            note = trim(line.substr(hash + 1));
            line = line.substr(0, hash);
        }
        line = trim(line);
        if (line.empty()) continue;
        std::optional<uint64_t> hz = parseFrequency(line);
        if (!hz)
        {
            list.problems.push_back("line " + std::to_string(number) + ": \"" + line + "\" is no frequency in MHz");
            continue;
        }
        list.entries.push_back(HotFrequency{*hz, note});
    }
    return list;
}

uint64_t stepFrequency(uint64_t hz, int notches, uint64_t stepHz)
{
    if (notches >= 0) return std::min(HIGHEST_HZ, hz + (uint64_t)notches * stepHz);
    const uint64_t down = (uint64_t)(-(int64_t)notches) * stepHz;
    return hz > down + LOWEST_HZ ? hz - down : std::min(hz, LOWEST_HZ);
}

std::vector<std::string> editorWords(const std::string& editorEnv, bool haveNano)
{
    std::vector<std::string> words;
    std::string word;
    for (char c : editorEnv)
    {
        if (std::isspace((unsigned char)c))
        {
            if (!word.empty()) words.push_back(word);
            word.clear();
        }
        else
        {
            word += c;
        }
    }
    if (!word.empty()) words.push_back(word);
    if (words.empty()) words.push_back(haveNano ? "nano" : "vi");
    return words;
}

namespace
{

// 'it', for a POSIX shell.
std::string shellQuoted(const std::string& s)
{
    std::string out = "'";
    for (char c : s)
    {
        if (c == '\'')
            out += "'\\''";
        else
            out += c;
    }
    return out + "'";
}

// "it", for AppleScript.
std::string appleQuoted(const std::string& s)
{
    std::string out = "\"";
    for (char c : s)
    {
        if (c == '\\' || c == '"') out += '\\';
        out += c;
    }
    return out + "\"";
}

struct Terminal
{
    const char* program;
    std::vector<std::string> before;   // the words before the command
    const char* desktop;               // $XDG_CURRENT_DESKTOP naming it first, or ""
};

} // namespace

std::vector<EditorLaunch> editorLaunches(Desktop desktop, const std::string& path, const std::string& editorEnv,
                                         const std::string& terminalEnv, const std::string& currentDesktop,
                                         bool haveNano)
{
    std::vector<EditorLaunch> out;
    if (desktop == Desktop::Windows)
    {
        // $EDITOR is rare on Windows; with one (vim, micro, "code -w"), it
        // runs in a console of its own, else Notepad.
        const std::string quotedPath = "\"" + path + "\"";
        if (!trim(editorEnv).empty())
        {
            EditorLaunch l;
            l.commandLine = "cmd.exe /c " + trim(editorEnv) + " " + quotedPath;
            l.console = true;
            out.push_back(l);
        }
        EditorLaunch notepad;
        notepad.commandLine = "notepad.exe " + quotedPath;
        out.push_back(notepad);
        return out;
    }
    if (desktop == Desktop::Mac)
    {
        // A program started from the Dock has no $EDITOR: Terminal's own
        // shell reads the operator's profile, so the editor is picked
        // there.
        const std::string command = "${EDITOR:-nano} " + shellQuoted(path) + "; exit";
        EditorLaunch terminal;
        terminal.program = "osascript";
        terminal.argv = {"osascript", "-e", "tell application \"Terminal\"", "-e", "activate",
                         "-e",        "do script " + appleQuoted(command),  "-e", "end tell"};
        terminal.nextIfFails = true;
        out.push_back(terminal);
        EditorLaunch textEdit;
        textEdit.program = "open";
        textEdit.argv = {"open", "-t", path};
        out.push_back(textEdit);
        return out;
    }

    std::vector<std::string> command = editorWords(editorEnv, haveNano);
    command.push_back(path);
    auto add = [&out, &command](const std::string& program, const std::vector<std::string>& before) {
        for (const EditorLaunch& l : out)
            if (l.program == program) return;
        EditorLaunch l;
        l.program = program;
        l.argv.push_back(program);
        l.argv.insert(l.argv.end(), before.begin(), before.end());
        l.argv.insert(l.argv.end(), command.begin(), command.end());
        out.push_back(l);
    };
    // Each terminal's way of being handed a command and its arguments.
    const std::vector<Terminal> terminals = {
        {"gnome-terminal", {"--"}, "GNOME"},
        {"ptyxis", {"--new-window", "--"}, ""},
        {"kgx", {"--"}, ""},
        {"konsole", {"-e"}, "KDE"},
        {"xfce4-terminal", {"-x"}, "XFCE"},
        {"mate-terminal", {"-x"}, "MATE"},
        {"lxterminal", {"-e"}, "LXDE"},
        {"kitty", {}, ""},
        {"alacritty", {"-e"}, ""},
        {"foot", {}, ""},
        {"wezterm", {"start", "--"}, ""},
        {"xterm", {"-e"}, ""},
    };
    // The operator's own pick first; most terminals take -e.
    const std::string own = trim(terminalEnv);
    if (!own.empty())
    {
        bool known = false;
        for (const Terminal& t : terminals)
        {
            if (own == t.program)
            {
                add(t.program, t.before);
                known = true;
            }
        }
        if (!known) add(own, {"-e"});
    }
    // Then the desktop's own terminal.
    const std::string desk = currentDesktop;
    for (const Terminal& t : terminals)
    {
        if (*t.desktop != '\0' && desk.find(t.desktop) != std::string::npos) add(t.program, t.before);
    }
    // Debian's and Ubuntu's alternative, set to the desktop's terminal.
    add("x-terminal-emulator", {"-e"});
    for (const Terminal& t : terminals) add(t.program, t.before);
    // No terminal at all: the desktop's text editor.
    EditorLaunch open;
    open.program = "xdg-open";
    open.argv = {"xdg-open", path};
    out.push_back(open);
    return out;
}

} // namespace radio
} // namespace ragtime
