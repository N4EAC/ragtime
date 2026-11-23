#include "StartupSplash.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

#include <wx/dcbuffer.h>
#include <wx/frame.h>
#include <wx/graphics.h>
#include <wx/stopwatch.h>
#include <wx/timer.h>

#include "ragtime/globus/Globe.h"

namespace
{
constexpr double PI = 3.14159265358979323846;
constexpr int DURATION_MS = 2800;
constexpr int FRAME_MS = 42;
constexpr double WIDTH = 420;
constexpr double HEIGHT = 450;
const wxColour BACKGROUND(8, 17, 27);

class StartupSplash final : public wxFrame
{
public:
    explicit StartupSplash(wxWindow* parent)
        : wxFrame(parent, wxID_ANY, "Ragtime", wxDefaultPosition, wxDefaultSize,
                  wxBORDER_NONE | wxFRAME_NO_TASKBAR | wxFRAME_FLOAT_ON_PARENT)
        , frames_(this)
        , lifetime_(this)
        , reduced_(startupMotionReduced())
    {
        SetName("Ragtime startup animation");
        SetClientSize(FromDIP(wxSize(int(WIDTH), int(HEIGHT))));
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(BACKGROUND);
        CentreOnParent();
        Bind(wxEVT_PAINT, &StartupSplash::paint, this);
        Bind(wxEVT_TIMER, [this](wxTimerEvent&) {
            if (clock_.Time() >= DURATION_MS) dismiss();
            else if (IsShownOnScreen()) Refresh(false);
        }, frames_.GetId());
        Bind(wxEVT_TIMER, [this](wxTimerEvent&) { dismiss(); }, lifetime_.GetId());
        Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) { dismiss(); });
        Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) {
            if (event.GetKeyCode() == WXK_ESCAPE) dismiss();
            else event.Skip();
        });
        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent&) { dismiss(); });
        clock_.Start();
        Show();
        lifetime_.StartOnce(DURATION_MS);
        if (!reduced_) frames_.Start(FRAME_MS);
    }

    ~StartupSplash() override
    {
        frames_.Stop();
        lifetime_.Stop();
    }

private:
    void dismiss()
    {
        if (closing_) return;
        closing_ = true;
        frames_.Stop();
        lifetime_.Stop();
        Destroy();
    }

    static void centred(wxGraphicsContext& gc, const wxString& text, double y,
                        const wxFont& font, const wxColour& colour)
    {
        gc.SetFont(font, colour);
        double width, height;
        gc.GetTextExtent(text, &width, &height);
        gc.DrawText(text, (WIDTH - width) / 2, y);
    }

    void binaryColour(wxGraphicsContext& gc, int alpha)
    {
        if (binaryColours_[0].IsNull())
            for (size_t i = 0; i < binaryColours_.size(); ++i)
                binaryColours_[i] = gc.CreateFont(binaryFont_, wxColour(105, 195, 255, int(i * 32 + 31)));
        gc.SetFont(binaryColours_[size_t(std::clamp(alpha / 32, 0, 7))]);
    }

    void stream(wxGraphicsContext& gc, double seconds, bool front)
    {
        constexpr const char* BITS = "01000110";
        for (int i = 0; i < 40; ++i)
        {
            const double angle = i * 2 * PI / 40 + seconds * 0.8;
            if ((std::sin(angle) >= 0) != front) continue;
            const double x = 176 * std::cos(angle);
            const double y = 57 * std::sin(angle);
            const double depth = (std::sin(angle) + 1) / 2;
            binaryColour(gc, int(65 + 175 * depth));
            gc.DrawText(wxString::Format("%c", BITS[i % 8]),
                        206 + x * 0.96 - y * 0.28, 174 + x * 0.28 + y * 0.96);
        }
    }

    void paint(wxPaintEvent&)
    {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(BACKGROUND));
        dc.Clear();
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc) return;
        gc->Scale(GetClientSize().x / WIDTH, GetClientSize().y / HEIGHT);
        gc->SetPen(wxPen(wxColour(31, 70, 80), 1));
        gc->SetBrush(gc->CreateLinearGradientBrush(0, 0, WIDTH, HEIGHT,
                                                  wxColour(19, 38, 51), BACKGROUND));
        gc->DrawRoundedRectangle(1, 1, WIDTH - 2, HEIGHT - 2, 24);
        const double seconds = reduced_ ? 0 : std::min(clock_.Time(), long(DURATION_MS)) / 1000.0;
        stream(*gc, seconds, false);

        gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(wxColour(83, 175, 250), 1.4)));
        gc->SetBrush(gc->CreateRadialGradientBrush(164, 127, 210, 181, 134,
                                                  wxColour(31, 115, 215), wxColour(6, 22, 58)));
        gc->DrawEllipse(78, 49, 264, 264);
        using namespace ragtime::globus;
        View view;
        view.radius = 132;
        view.attitude = lookingAt(toVector({20, -48 + seconds * 18}));
        const Window window{WIDTH, 362};
        auto land = gc->CreatePath();
        for (const auto& outline : landOutlines(view, window))
        {
            if (outline.empty()) continue;
            land.MoveToPoint(outline.front().x, outline.front().y);
            for (size_t i = 1; i < outline.size(); ++i) land.AddLineToPoint(outline[i].x, outline[i].y);
            land.CloseSubpath();
        }
        gc->SetBrush(wxBrush(wxColour(68, 165, 246, 115)));
        gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(wxColour(130, 218, 255, 180), 0.6)));
        gc->DrawPath(land, wxWINDING_RULE);
        auto grid = gc->CreatePath();
        for (const auto& line : graticule(view, window, 30))
        {
            if (line.empty()) continue;
            grid.MoveToPoint(line.front().x, line.front().y);
            for (size_t i = 1; i < line.size(); ++i) grid.AddLineToPoint(line[i].x, line[i].y);
        }
        gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(wxColour(111, 180, 250, 45), 0.6)));
        gc->StrokePath(grid);

        // Digits follow the sphere's surface; their opacity fades towards its right rim.
        for (int row = -4; row <= 4; ++row)
        {
            const double latitude = row * PI / 12;
            for (int col = 0; col < 6; ++col)
            {
                const double longitude = std::fmod(col * 0.23 + seconds * 0.18, 1.43);
                const double x = std::cos(latitude) * std::sin(longitude);
                const int alpha = int(175 * std::sin(longitude * PI / 1.43));
                binaryColour(*gc, alpha);
                gc->DrawText((row + col) % 2 ? "0" : "1", 208 + x * 129, 174 - std::sin(latitude) * 129);
            }
        }
        // A small static highlight gives the vector globe a glass surface.
        gc->SetPen(wxPen(wxColour(183, 255, 244, 110), 2));
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->DrawEllipse(81, 52, 258, 258);
        stream(*gc, seconds, true);

        centred(*gc, "Ragtime", 338, GetFont().Larger().Larger().Bold(), wxColour(239, 255, 253));
        centred(*gc, "for macOS", 376, GetFont(), wxColour(161, 206, 250));
    }

    std::array<wxGraphicsFont, 8> binaryColours_;
    const wxFont binaryFont_{wxFontInfo(10).Family(wxFONTFAMILY_TELETYPE)};
    wxTimer frames_;
    wxTimer lifetime_;
    wxStopWatch clock_;
    bool reduced_;
    bool closing_ = false;
};
} // namespace

void showStartupSplash(wxWindow* parent)
{
    new StartupSplash(parent);
}
