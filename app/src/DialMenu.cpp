#include "DialMenu.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include <wx/button.h>
#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/process.h>
#include <wx/textdlg.h>
#include <wx/utils.h>
#include <wx/window.h>

#include "HostConnection.h"
#include "ragtime/radio/Bands.h"

using namespace ragtime;

namespace
{

// The file is looked at this often.
constexpr uint64_t LOOK_MS = 2000;
// A tune not read back in this long: the dial shows what the rig says.
constexpr uint64_t PENDING_MS = 3000;

wxString utf8(const std::string& s)
{
    return wxString::FromUTF8(s);
}

std::string env(const char* name)
{
    wxString value;
    return wxGetEnv(name, &value) ? std::string(value.utf8_str()) : std::string();
}

// The program on the PATH (or named with its path).
bool found(const std::string& program)
{
    if (program.empty()) return true;
    if (program.find('/') != std::string::npos) return wxFileName::IsFileExecutable(utf8(program));
    wxPathList paths;
    paths.AddEnvList("PATH");
    return !paths.FindAbsoluteValidPath(utf8(program)).empty();
}

radio::Desktop desktop()
{
#if defined(__WXMSW__)
    return radio::Desktop::Windows;
#elif defined(__WXMAC__) || defined(__APPLE__)
    return radio::Desktop::Mac;
#else
    return radio::Desktop::Linux;
#endif
}

// An editor that failed (osascript refused Terminal): the next way.
class Watched : public wxProcess
{
public:
    explicit Watched(std::function<void(int)> ended) : ended_(std::move(ended)) {}
    void OnTerminate(int, int status) override
    {
        if (ended_) ended_(status);
        delete this;
    }

private:
    std::function<void(int)> ended_;
};

} // namespace

DialMenu::DialMenu(wxButton* button, std::string path, Rig rig)
    : button_(button)
    , path_(std::move(path))
    , rig_(std::move(rig))
{
    list_ = radio::parseHotFrequencies(radio::HOT_FREQUENCIES_SEED);
    load(true);
    button_->Bind(wxEVT_BUTTON, &DialMenu::onClick, this);
    button_->Bind(wxEVT_CONTEXT_MENU, &DialMenu::onContext, this);
    button_->Bind(wxEVT_MOUSEWHEEL, &DialMenu::onWheel, this);
    // GTK hands a button's wheel to the panel under it: taken there when
    // the pointer is over the dial.
    if (wxWindow* parent = button_->GetParent()) parent->Bind(wxEVT_MOUSEWHEEL, &DialMenu::onBarWheel, this);
}

DialMenu::~DialMenu()
{
    *alive_ = false;
}

void DialMenu::load(bool force)
{
    wxFileName file(utf8(path_));
    if (!file.FileExists())
    {
        // Never written, or taken away: the seed list.
        if (fromFile_ || force)
        {
            list_ = radio::parseHotFrequencies(radio::HOT_FREQUENCIES_SEED);
            fromFile_ = false;
            readProblem_.clear();
        }
        return;
    }
    wxDateTime time = file.GetModificationTime();
    wxULongLong size = file.GetSize();
    if (!force && fromFile_ && time.IsValid() && seenTime_.IsValid() && time == seenTime_ && size == seenSize_)
        return;
    wxFFile in(file.GetFullPath(), "rb");
    std::string text;
    if (!in.IsOpened())
    {
        readProblem_ = "Ragtime can't read " + path_;
        return;
    }
    char buffer[4096];
    size_t got = 0;
    while ((got = in.Read(buffer, sizeof buffer)) > 0 && text.size() < 1000000) text.append(buffer, got);
    seenTime_ = time;
    seenSize_ = size;
    fromFile_ = true;
    readProblem_.clear();
    list_ = radio::parseHotFrequencies(text);
}

void DialMenu::poll()
{
    uint64_t now = HostConnection::nowMs();
    if (now < lookedMs_ + LOOK_MS) return;
    lookedMs_ = now;
    load(false);
}

std::optional<uint64_t> DialMenu::pending(std::optional<uint64_t> dial)
{
    if (!target_) return std::nullopt;
    if ((dial && *dial == *target_) || HostConnection::nowMs() > targetMs_ + PENDING_MS)
    {
        target_.reset();
        return std::nullopt;
    }
    return target_;
}

std::string DialMenu::tip() const
{
    std::string tip = "Click for your hot frequencies and tune the rig to one. Right-click to edit the list or type "
                      "a frequency (Custom). Ctrl with the mouse wheel steps 1 kHz, Shift 10 kHz.\n"
                      "The list: " + path_ + (fromFile_ ? "" : " (Ragtime's starting list until you edit it)");
    if (!readProblem_.empty()) tip += "\n" + readProblem_;
    for (size_t i = 0; i < list_.problems.size() && i < 5; i++) tip += "\nSkipped, " + list_.problems[i];
    if (list_.problems.size() > 5) tip += "\n... and " + std::to_string(list_.problems.size() - 5) + " more";
    return tip;
}

void DialMenu::onClick(wxCommandEvent&)
{
    showMenu(true);
}

void DialMenu::onContext(wxContextMenuEvent&)
{
    showMenu(false);
}

void DialMenu::showMenu(bool full)
{
    load(false);
    enum
    {
        ID_CUSTOM = 1,
        ID_EDIT,
        ID_FIRST,   // the list's first entry, then the rest in turn
    };
    wxMenu menu;
    const bool can = rig_.canTune && rig_.canTune();
    const std::optional<uint64_t> dial = rig_.dialHz ? rig_.dialHz() : std::nullopt;
    if (full)
    {
        if (!can)
        {
            menu.Append(wxID_ANY, "The rig isn't answering: Ragtime can't tune it now")->Enable(false);
            menu.AppendSeparator();
        }
        int id = ID_FIRST;
        for (const radio::HotFrequency& f : list_.entries)
        {
            // Plain items, the one the rig is on marked with a tick: a
            // check item would put an empty box beside every other one.
            // The rest are indented by a figure space and a space, about
            // the tick's width, so the frequencies line up.
            const bool current = dial && *dial == f.hz;
            std::string label = (current ? "\xe2\x9c\x93 " : "\xe2\x80\x87\xe2\x80\x82") + radio::formatMHz(f.hz);
            if (!f.note.empty()) label += " \xc2\xb7 " + f.note;
            wxString text = utf8(label);
            text.Replace("&", "&&");
            menu.Append(id++, text)->Enable(can);
        }
        if (list_.entries.empty()) menu.Append(wxID_ANY, "No frequencies in the list: Edit... to add some")->Enable(false);
        menu.AppendSeparator();
    }
    menu.Append(ID_EDIT, wxString::FromUTF8("Edit\xe2\x80\xa6"));
    menu.Append(ID_CUSTOM, wxString::FromUTF8("Custom\xe2\x80\xa6"))->Enable(can);
    const int picked = button_->GetPopupMenuSelectionFromUser(menu, full ? wxPoint(0, button_->GetSize().y)
                                                                         : wxDefaultPosition);
    if (picked == ID_CUSTOM)
    {
        custom();
    }
    else if (picked == ID_EDIT)
    {
        edit();
    }
    else if (picked >= ID_FIRST && picked < ID_FIRST + (int)list_.entries.size())
    {
        tuneTo(list_.entries[(size_t)(picked - ID_FIRST)].hz, "list");
    }
}

void DialMenu::tuneTo(uint64_t hz, const std::string& how)
{
    if (!rig_.tune || !(rig_.canTune && rig_.canTune())) return;
    target_ = hz;
    targetMs_ = HostConnection::nowMs();
    rig_.tune(hz, how);
}

void DialMenu::custom()
{
    std::optional<uint64_t> base = pending(rig_.dialHz ? rig_.dialHz() : std::nullopt);
    if (!base && rig_.dialHz) base = rig_.dialHz();
    wxString value = base ? utf8(radio::formatMHz(*base)) : wxString();
    for (;;)
    {
        wxTextEntryDialog ask(button_, "Tune the rig to (MHz, or say kHz or Hz):", "Custom frequency", value);
        if (ask.ShowModal() != wxID_OK) return;
        value = ask.GetValue();
        if (std::optional<uint64_t> hz = radio::parseFrequency(std::string(value.utf8_str())))
        {
            tuneTo(*hz, "custom");
            return;
        }
        wxMessageBox("\"" + value + "\" isn't a frequency Ragtime can tune to. Type it in MHz, such as 7.066, or "
                     "say kHz or Hz, such as 7066 kHz.",
                     "Custom frequency", wxOK | wxICON_WARNING, button_);
    }
}

void DialMenu::onBarWheel(wxMouseEvent& event)
{
    if (button_->IsShown() && button_->GetRect().Contains(event.GetPosition()))
        onWheel(event);
    else
        event.Skip();
}

void DialMenu::onWheel(wxMouseEvent& event)
{
    // Ctrl (or Command on a Mac) for 1 kHz, Shift for 10 kHz; a wheel
    // with neither does what it would anyway.
    const bool shift = event.ShiftDown();
    const bool control = event.ControlDown() || event.RawControlDown();
    if (!shift && !control)
    {
        event.Skip();
        return;
    }
    const std::optional<uint64_t> dial = rig_.dialHz ? rig_.dialHz() : std::nullopt;
    std::optional<uint64_t> from = pending(dial);
    if (!from) from = dial;
    if (!from || !(rig_.canTune && rig_.canTune())) return;
    // A touchpad's small turns add up to notches. A Mac turns Shift with the
    // wheel into the other axis; it counts the same.
    const int delta = event.GetWheelDelta() > 0 ? event.GetWheelDelta() : 120;
    wheelRest_ += event.GetWheelRotation();
    const int notches = wheelRest_ / delta;
    wheelRest_ -= notches * delta;
    if (notches == 0) return;
    const uint64_t hz =
        radio::stepFrequency(*from, notches, shift ? radio::WHEEL_SHIFT_STEP_HZ : radio::WHEEL_STEP_HZ);
    if (hz != *from) tuneTo(hz, "wheel");
}

bool DialMenu::ensureFile(std::string& why)
{
    wxFileName file(utf8(path_));
    if (file.FileExists()) return true;
    if (!wxFileName::DirExists(file.GetPath()) && !wxFileName::Mkdir(file.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL))
    {
        why = "Ragtime couldn't make the folder " + std::string(file.GetPath().utf8_str());
        return false;
    }
    wxFFile out(file.GetFullPath(), "wb");
    const std::string seed = radio::HOT_FREQUENCIES_SEED;
    if (!out.IsOpened() || !out.Write(seed.data(), seed.size()) || !out.Close())
    {
        why = "Ragtime couldn't write " + path_;
        return false;
    }
    load(true);
    return true;
}

void DialMenu::edit()
{
    std::string why;
    if (!ensureFile(why))
    {
        wxMessageBox(utf8(why), "Hot frequencies", wxOK | wxICON_WARNING, button_);
        return;
    }
    launches_ = radio::editorLaunches(desktop(), path_, env("EDITOR"), env("TERMINAL"), env("XDG_CURRENT_DESKTOP"),
                                      found("nano"));
    if (!launch(0))
        wxMessageBox(utf8("Ragtime found no terminal or text editor to open the list with. It is a plain text file, "
                          "one frequency in MHz per line:\n\n" + path_),
                     "Hot frequencies", wxOK | wxICON_INFORMATION, button_);
}

bool DialMenu::launch(size_t from)
{
    for (size_t i = from; i < launches_.size(); i++)
    {
        const radio::EditorLaunch& l = launches_[i];
        if (!found(l.program)) continue;
        Watched* watch = nullptr;
        if (l.nextIfFails)
        {
            std::shared_ptr<bool> alive = alive_;
            watch = new Watched([this, alive, i](int status) {
                if (status != 0 && *alive) launch(i + 1);
            });
        }
        long pid = 0;
        if (!l.commandLine.empty())
        {
            int flags = wxEXEC_ASYNC | (l.console ? wxEXEC_SHOW_CONSOLE : wxEXEC_HIDE_CONSOLE);
            pid = wxExecute(utf8(l.commandLine), flags, watch);
        }
        else
        {
            std::vector<wxString> words;
            for (const std::string& a : l.argv) words.push_back(utf8(a));
            std::vector<const wchar_t*> argv;
            for (const wxString& w : words) argv.push_back(w.wc_str());
            argv.push_back(nullptr);
            pid = wxExecute(argv.data(), wxEXEC_ASYNC, watch);
        }
        if (pid != 0) return true;
        // Not started: wx deletes no process it never ran.
        delete watch;
    }
    return false;
}
