#include <string>
#include <vector>

#include <wx/app.h>
#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/utils.h>

#include "Check.h"
#include "EmbeddedModem.h"
#include "ModemMonitor.h"

namespace
{
class TestApp : public wxApp {};

template<class T> T* find(wxWindow* parent)
{
    for (auto* child : parent->GetChildren())
    {
        if (auto* match = dynamic_cast<T*>(child)) return match;
        if (auto* match = find<T>(child)) return match;
    }
    return nullptr;
}

void settle()
{
    for (int i = 0; i < 10; ++i)
    {
        wxTheApp->ProcessPendingEvents();
        wxYield();
        wxMilliSleep(10);
    }
}

int run(int argc, char** argv)
{
    wxApp::SetInstance(new TestApp);
    if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) return 77;
    int result = 0;
    {
        // No station is started: these are presentation fixtures, with no
        // access to audio devices, sockets, operator settings or a radio.
        EmbeddedModem modem([](const std::string&) {});
        CHECK(modem.takeBursts().empty());
        auto* monitor = new ModemMonitor(nullptr, modem, false);
        monitor->setOpen(true);
        settle();
        auto* list = find<wxListCtrl>(monitor);
        auto* clear = find<wxButton>(monitor);
        CHECK(list != nullptr && clear != nullptr);
        if (list && clear)
        {
            CHECK_EQ(list->GetColumnCount(), 4);
            CHECK(!clear->IsEnabled());
            monitor->appendBursts({
                {"OK mode", false, -4.5, 1000, 3, 3, 0},
                {"Lost mode", true, {}, 2000, 2, 0, 0},
                {"Undecoded mode", false, {}, 3000, 1, 0, 0},
                {"Partial mode", false, 2.0, 4000, 3, 2, 1},
                {"Other station", false, {}, 5000, 3, 0, 3},
            });
            CHECK_EQ(list->GetItemCount(), 5);
            CHECK(list->GetItemText(0, 3) == "Undecoded");
            CHECK(list->GetItemText(1, 3) == "Incomplete data (2/3)");
            CHECK(list->GetItemText(2, 3) == "Undecoded");
            CHECK(list->GetItemText(3, 3) == "Lost");
            CHECK(list->GetItemText(4, 3) == "OK");
            CHECK(list->GetItemText(4, 0) == "00:00:01");
            CHECK(list->GetItemText(4, 1) == "OK mode x3");
            CHECK(list->GetItemText(4, 2) == "-4.5");
            CHECK(list->GetItemText(3, 2) == wxString::FromUTF8("—"));
            monitor->appendBursts({});
            CHECK_EQ(list->GetItemCount(), 5);
            std::vector<ModemBurst> many;
            for (int i = 0; i < 600; ++i)
                many.push_back({"burst" + std::to_string(i), false, {}, 6000 + i, 1, 1, 0});
            monitor->appendBursts(many);
            CHECK_EQ(list->GetItemCount(), 500);
            CHECK(list->GetItemText(0, 1) == "burst599 x1");
            CHECK(list->GetItemText(499, 1) == "burst100 x1");
            monitor->SetClientSize(monitor->FromDIP(wxSize(700, 490)));
            settle();
            int width = 0;
            for (int col = 0; col < 4; ++col) width += list->GetColumnWidth(col);
            CHECK(width <= list->GetClientSize().x);
            CHECK(clear->IsEnabled());
            wxCommandEvent event(wxEVT_BUTTON, clear->GetId());
            clear->ProcessWindowEvent(event);
            CHECK_EQ(list->GetItemCount(), 0);
            CHECK(!clear->IsEnabled());
            monitor->appendBursts({{"Fresh", false, {}, 10000, 1, 1, 0}});
            monitor->setOpen(false);
            monitor->setOpen(true);
            CHECK_EQ(list->GetItemCount(), 1);
            monitor->reset("");
            CHECK_EQ(list->GetItemCount(), 1); // Keep history when the modem stops.
        }
        monitor->setOpen(false);
        monitor->Destroy();
        settle();
        result = ragtime_test::finish();
    }
    wxEntryCleanup();
    return result;
}
} // namespace

int main(int argc, char** argv)
{
    return EmbeddedModem::runWithRuntime(argc, argv, run);
}
