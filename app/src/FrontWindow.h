#ifndef RAGTIME_FRONT_WINDOW_H
#define RAGTIME_FRONT_WINDOW_H

#include <wx/toplevel.h>
#include <wx/window.h>

// Whether a window's top-level window is the active one. In GTK, giving a
// control the focus while its window is not in front presents the window
// (wxWindow::SetFocus calls gtk_window_present), bringing it up over the
// others; and wxNotebook gives a page the focus whenever it shows one. So
// a tab brought up by something heard (a session starting) lifted the main
// window over the Ear (Jeff, 2026-11-15: it "jumps into the background").
// The main window's pages take the focus only when this says yes.
inline bool inFrontWindow(wxWindow* window)
{
    auto* top = dynamic_cast<wxTopLevelWindow*>(wxGetTopLevelParent(window));
    return top && top->IsActive();
}

#endif
