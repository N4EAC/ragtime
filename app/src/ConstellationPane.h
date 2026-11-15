#ifndef RAGTIME_CONSTELLATION_PANE_H
#define RAGTIME_CONSTELLATION_PANE_H

#include <wx/panel.h>

#include "EmbeddedModem.h"

class wxPaintEvent;

class ConstellationPane : public wxPanel
{
public:
    explicit ConstellationPane(wxWindow* parent);
    bool AcceptsFocus() const override { return false; }

    void clear();
    void setPlot(ModemConstellation plot);

private:
    void paint(wxPaintEvent&);

    std::optional<ModemConstellation> plot_;
};

#endif
