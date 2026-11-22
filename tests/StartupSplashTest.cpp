#include <cstdio>
#include <ctime>

#include <wx/app.h>
#include <wx/frame.h>
#include <wx/stopwatch.h>
#include <wx/timer.h>
#include <wx/utils.h>

#include "Check.h"
#include "StartupSplash.h"

namespace
{
bool reduced = false;

void pump(int milliseconds)
{
    wxStopWatch elapsed;
    while (elapsed.Time() < milliseconds)
    {
        wxYield();
        wxMilliSleep(5);
    }
}

wxWindow* splash(wxWindow* parent)
{
    for (auto* child : parent->GetChildren())
        if (child->GetName() == "Ragtime startup animation") return child;
    return nullptr;
}
} // namespace

bool startupMotionReduced() { return reduced; }

class StartupTestApp : public wxApp
{
public:
    bool OnInit() override { SetExitOnFrameDelete(false); return true; }
};
wxIMPLEMENT_APP_NO_MAIN(StartupTestApp);

int main(int argc, char** argv)
{
    if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) return 77;
    auto* parent = new wxFrame(nullptr, wxID_ANY, "Startup animation check", wxDefaultPosition, wxSize(800, 650));
    parent->Show();
    pump(100);
    int paints = 0;
    int backgroundTicks = 0;
    wxTimer background(parent);
    parent->Bind(wxEVT_TIMER, [&](wxTimerEvent&) { ++backgroundTicks; }, background.GetId());
    background.Start(20);
    const auto cpuStart = std::clock();
    showStartupSplash(parent);
    CHECK(splash(parent) != nullptr);
    splash(parent)->Bind(wxEVT_PAINT, [&](wxPaintEvent& event) { ++paints; event.Skip(); });
    pump(850);
    CHECK(splash(parent) != nullptr);
    CHECK(backgroundTicks > 10);
    pump(2200);
    CHECK(splash(parent) == nullptr);
    CHECK(paints > 15 && paints < 90);
    std::printf("Animation: %d paints, %d background ticks, %.1f ms process CPU over 3.05 seconds\n",
                paints, backgroundTicks, 1000.0 * (std::clock() - cpuStart) / CLOCKS_PER_SEC);

    showStartupSplash(parent);
    wxKeyEvent escape(wxEVT_CHAR_HOOK);
    escape.m_keyCode = WXK_ESCAPE;
    splash(parent)->ProcessWindowEvent(escape);
    pump(80);
    CHECK(splash(parent) == nullptr);

    showStartupSplash(parent);
    wxMouseEvent click(wxEVT_LEFT_DOWN);
    splash(parent)->ProcessWindowEvent(click);
    pump(80);
    CHECK(splash(parent) == nullptr);

    reduced = true;
    showStartupSplash(parent);
    paints = 0;
    splash(parent)->Bind(wxEVT_PAINT, [&](wxPaintEvent& event) { ++paints; event.Skip(); });
    pump(1000);
    CHECK(splash(parent) != nullptr);
    CHECK(paints < 5);
    pump(2050);
    CHECK(splash(parent) == nullptr);

    showStartupSplash(parent);
    background.Stop();
    parent->Destroy();
    pump(150);
    wxEntryCleanup();
    return ragtime_test::finish();
}
