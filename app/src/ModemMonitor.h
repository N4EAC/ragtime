#ifndef RAGTIME_MODEM_MONITOR_H
#define RAGTIME_MODEM_MONITOR_H

#include <vector>

#include <wx/frame.h>
#include <wx/timer.h>

class ConstellationPane;
class EmbeddedModem;
class wxStaticText;
class wxListCtrl;
class wxButton;
struct ModemBurst;

class ModemMonitor : public wxFrame
{
public:
    ModemMonitor(wxWindow* parent, EmbeddedModem& modem, bool onTop);
    void setOpen(bool open);
    void reset(const wxString& error);
    void appendBursts(const std::vector<ModemBurst>& bursts);

private:
    void poll();
    void fitColumns();

    EmbeddedModem& modem_;
    wxTimer timer_;
    ConstellationPane* plot_;
    wxListCtrl* attempts_;
    wxButton* clear_;
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
