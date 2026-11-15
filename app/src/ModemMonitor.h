#ifndef RAGTIME_MODEM_MONITOR_H
#define RAGTIME_MODEM_MONITOR_H

#include <wx/frame.h>
#include <wx/timer.h>

class ConstellationPane;
class EmbeddedModem;
class wxStaticText;

class ModemMonitor : public wxFrame
{
public:
    ModemMonitor(wxWindow* parent, EmbeddedModem& modem, bool onTop);
    void setOpen(bool open);
    void reset(const wxString& error);

private:
    void poll();

    EmbeddedModem& modem_;
    wxTimer timer_;
    ConstellationPane* plot_;
    wxStaticText* status_;
    wxStaticText* channel_;
    wxStaticText* ptt_;
    wxStaticText* link_;
    wxStaticText* mode_;
    wxStaticText* traffic_;
    wxStaticText* burst_;
    wxStaticText* capture_;
    wxStaticText* playback_;
    wxStaticText* decode_;
};

#endif
