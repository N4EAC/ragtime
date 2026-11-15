#include "ConstellationPane.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <wx/settings.h>

ConstellationPane::ConstellationPane(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
{
    SetName("Data2G receive constellation");
    SetMinSize(FromDIP(wxSize(220, 220)));
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &ConstellationPane::paint, this);
}

void ConstellationPane::clear()
{
    if (!plot_) return;
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
    std::unique_ptr<wxGraphicsContext> graphics(wxGraphicsContext::Create(dc));
    if (!graphics) return;
    auto& canvas = *graphics;
    canvas.SetAntialiasMode(wxANTIALIAS_NONE);

    const wxSize size = GetClientSize();
    const int side = std::min(size.x, size.y);
    if (side <= 0) return;
    const int left = (size.x - side) / 2;
    canvas.Translate(left, 0);
    canvas.Clip(0, 0, side, side);
    canvas.SetPen(*wxTRANSPARENT_PEN);
    canvas.SetBrush(*wxBLACK_BRUSH);
    canvas.DrawRectangle(0, 0, side, side);

    // Follow Data2G v0.5.1 native/gui/constellation.cpp. The station supplies
    // both the received symbols and the ideal points, including the CPM tone wheel.
    const double mid = side / 2.0;
    canvas.SetPen(wxPen(wxColour(60, 60, 60)));
    canvas.StrokeLine(mid, 0, mid, side);
    canvas.StrokeLine(0, mid, side, mid);
    if (!plot_) return;

    double reach = 0.0;
    for (const auto& point : plot_->points)
        reach = std::max({reach, double(std::abs(point.real())), double(std::abs(point.imag()))});
    const double scale = mid / (reach > 0 ? 1.5 * reach : 1.5);
    const auto at = [&](std::complex<float> point) {
        return wxPoint2DDouble(mid + point.real() * scale, mid - point.imag() * scale);
    };
    if (plot_->tones)
    {
        canvas.SetBrush(*wxTRANSPARENT_BRUSH);
        canvas.DrawEllipse(mid - scale, mid - scale, 2 * scale, 2 * scale);
    }

    canvas.SetPen(*wxTRANSPARENT_PEN);
    const auto count = plot_->symbols.size();
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto point = at(plot_->symbols[i]);
        // Outliers and nonfinite estimates cannot contribute a visible pixel.
        if (!(point.m_x >= 0 && point.m_x < side && point.m_y >= 0 && point.m_y < side)) continue;
        const double t = plot_->tones && count > 1 ? double(i) / double(count - 1) : 1.0;
        canvas.SetBrush(wxBrush(wxColour(0, int(120 + 135 * t), int(255 - 135 * t), 140)));
        canvas.DrawRectangle(std::floor(point.m_x), std::floor(point.m_y), 1, 1);
    }

    canvas.SetPen(wxPen(wxColour(255, 80, 80)));
    for (const auto& point : plot_->points)
    {
        if (plot_->tones)
        {
            const auto inner = at(point * 1.06f);
            const auto outer = at(point * 1.16f);
            canvas.StrokeLine(inner.m_x, inner.m_y, outer.m_x, outer.m_y);
        }
        else
        {
            const auto center = at(point);
            canvas.StrokeLine(center.m_x - 3, center.m_y, center.m_x + 3, center.m_y);
            canvas.StrokeLine(center.m_x, center.m_y - 3, center.m_x, center.m_y + 3);
        }
    }

    auto label = wxString::FromUTF8(plot_->mode);
    if (plot_->live) label += " (live)";
    else if (plot_->lost) label += " (lost)";
    canvas.SetFont(GetFont(), wxColour(128, 128, 128));
    canvas.DrawText(label, 4, 2);
}
