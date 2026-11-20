// Creating a simplebook page gives it focus on Cocoa before its first
// size event. An empty Recent work list must already have a valid range,
// or wxWidgets can walk an uninitialized number of rows during startup.
#include <wx/app.h>
#include <wx/frame.h>
#include <wx/htmllbox.h>
#include <wx/sizer.h>
#include <wx/utils.h>

#include "Check.h"
#include "RecentWorkPane.h"

class RecentWorkTestApp : public wxApp
{
public:
    bool OnInit() override { return true; }
};

wxIMPLEMENT_APP_NO_MAIN(RecentWorkTestApp);

wxHtmlListBox* findList(wxWindow* parent)
{
    if (auto* list = wxDynamicCast(parent, wxHtmlListBox)) return list;
    for (wxWindowList::compatibility_iterator child = parent->GetChildren().GetFirst(); child; child = child->GetNext())
        if (auto* list = findList(child->GetData())) return list;
    return nullptr;
}

int main(int argc, char** argv)
{
    if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) return 77;
    auto* frame = new wxFrame(nullptr, wxID_ANY, "Recent work", wxDefaultPosition, wxSize(420, 300));
    auto* pane = new RecentWorkPane(frame);
    auto* list = findList(pane);
    CHECK(list != nullptr);
    if (!list) return ragtime_test::finish();
    CHECK_EQ(list->GetItemCount(), (size_t)0);
    CHECK_EQ(list->GetVisibleRowsEnd(), (size_t)0);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(pane, 1, wxEXPAND);
    frame->SetSizer(sizer);
    frame->Show();
    wxYield();
    CHECK(pane->selected().empty());

    // Exercise focus and repaint when an empty pane gains its first row,
    // then returns to empty, as a fresh station hears its first burst.
    ragtime::radio::RecentWork work;
    ragtime::radio::WorkRow row;
    row.call = "W1AW";
    row.heardDays = 1;
    work.rows.push_back(row);
    pane->setWork(work, 1000);
    list->SetFocus();
    wxYield();
    CHECK_EQ(list->GetItemCount(), (size_t)1);
    pane->setWork({}, 2000);
    wxYield();
    CHECK_EQ(list->GetItemCount(), (size_t)0);
    CHECK_EQ(list->GetVisibleRowsEnd(), (size_t)0);
    CHECK(pane->selected().empty());
    CHECK(frame->IsShown());
    frame->Destroy();
    wxTheApp->ProcessPendingEvents();
    wxEntryCleanup();
    return ragtime_test::finish();
}
