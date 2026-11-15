#include "ConstellationPane.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <wx/dcbuffer.h>
#include <wx/settings.h>

#include "Html.h"

ConstellationPane::ConstellationPane(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
{
    SetName("Data2G constellation");
    SetMinSize(wxSize(-1, FromDIP(124)));
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &ConstellationPane::paint, this);
}

void ConstellationPane::clear(bool modemRunning)
{
    modemRunning_ = modemRunning;
    plot_.reset();
    Refresh();
}

void ConstellationPane::setPlot(ModemConstellation plot)
{
    plot_ = std::move(plot);
    Refresh();
}

void ConstellationPane::paint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)));
    dc.Clear();

    const wxSize size = GetClientSize();
    const int pad = FromDIP(6);
    dc.SetFont(GetFont().Bold());
    dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    dc.DrawText("Data2G constellation", pad, pad);
    const int top = pad * 2 + dc.GetCharHeight();
    const int side = std::min(size.y - top - pad, size.x / 2 - pad);
    if (side <= 0) return;
    dc.SetFont(GetFont());
    dc.SetTextForeground(secondaryText());
    wxString status = modemRunning_ ? "Waiting for RX" : "Modem offline";
    if (plot_)
    {
        status = wxString::FromUTF8(plot_->mode);
        if (plot_->live) status += " (live)";
        else if (plot_->lost) status += " (lost)";
    }
    dc.DrawText(status, pad * 2 + side, top);

    const wxRect square(pad, top, side, side);
    wxDCClipper clip(dc, square);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(wxColour(13, 18, 27)));
    dc.DrawRectangle(square);
    const wxPoint center(pad + side / 2, top + side / 2);
    dc.SetPen(wxPen(wxColour(66, 77, 93)));
    dc.DrawLine(square.x, center.y, square.GetRight() + 1, center.y);
    dc.DrawLine(center.x, square.y, center.x, square.GetBottom() + 1);
    if (!plot_) return;

    // Use the modem's ideal points for the scale, so noise does not zoom the plot.
    double reach = 0.0;
    for (const auto& point : plot_->points)
        reach = std::max({reach, double(std::abs(point.real())), double(std::abs(point.imag()))});
    const double scale = side / (3.0 * (reach > 0 ? reach : 1.0));
    const auto at = [&](std::complex<float> z) {
        return wxPoint(center.x + int(std::lround(z.real() * scale)),
                       center.y - int(std::lround(z.imag() * scale)));
    };
    if (plot_->tones)
    {
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawCircle(center, int(std::lround(scale)));
    }

    const auto count = plot_->symbols.size();
    constexpr int lastShade = 15;
    int shade = -1;
    dc.SetPen(wxPen(wxColour(92, 210, 247), FromDIP(2)));
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto symbol = plot_->symbols[i];
        // Reject nonfinite and offscreen points before converting to pixels.
        if (!(std::abs(symbol.real() * scale) < side / 2.0 &&
              std::abs(symbol.imag() * scale) < side / 2.0)) continue;
        if (plot_->tones && count > 1)
        {
            // The same early-blue to late-green cue as the Data2G GUI.
            const int nextShade = int(i * lastShade / (count - 1));
            if (shade != nextShade)
            {
                shade = nextShade;
                const int tint = 100 * shade / lastShade;
                dc.SetPen(wxPen(wxColour(70, 132 + tint, 244 - tint), FromDIP(2)));
            }
        }
        dc.DrawPoint(at(symbol));
    }

    dc.SetPen(wxPen(wxColour(255, 135, 122)));
    for (const auto& point : plot_->points)
    {
        if (plot_->tones)
            dc.DrawLine(at(point * 1.06f), at(point * 1.16f));
        else
        {
            const auto pixel = at(point);
            const int arm = FromDIP(3);
            dc.DrawLine(pixel.x - arm, pixel.y, pixel.x + arm + 1, pixel.y);
            dc.DrawLine(pixel.x, pixel.y - arm, pixel.x, pixel.y + arm + 1);
        }
    }
}
