#include "ModemMonitor.h"

#include <algorithm>
#include <utility>

#include <wx/button.h>
#include <wx/datetime.h>
#include <wx/listctrl.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "ConstellationPane.h"
#include "EmbeddedModem.h"

namespace
{
constexpr int POLL_MS = 150;
constexpr long MAX_BURSTS = 500;

void update(wxStaticText* field, const wxString& value)
{
    if (field->GetLabelText() != value) field->SetLabelText(value);
}
} // namespace

ModemMonitor::ModemMonitor(wxWindow* parent, EmbeddedModem& modem, bool onTop)
    : wxFrame(parent, wxID_ANY, "Data2G modem monitor", wxDefaultPosition, wxDefaultSize,
              wxDEFAULT_FRAME_STYLE | (onTop ? wxFRAME_FLOAT_ON_PARENT : 0))
    , modem_(modem)
    , timer_(this)
{
    auto* panel = new wxPanel(this);
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    auto* column = new wxBoxSizer(wxVERTICAL);
    auto* fields = new wxFlexGridSizer(2, FromDIP(8), FromDIP(10));
    fields->AddGrowableCol(1);

    status_ = new wxStaticText(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                               wxST_NO_AUTORESIZE | wxST_ELLIPSIZE_END);
    status_->SetFont(status_->GetFont().Bold());
    column->Add(status_, 0, wxEXPAND | wxBOTTOM, FromDIP(12));
    const auto addField = [&](const char* name) {
        fields->Add(new wxStaticText(panel, wxID_ANY, name), 0, wxALIGN_CENTER_VERTICAL);
        auto* value = new wxStaticText(panel, wxID_ANY, "—", wxDefaultPosition, wxDefaultSize,
                                       wxST_NO_AUTORESIZE | wxST_ELLIPSIZE_END);
        fields->Add(value, 0, wxEXPAND);
        return value;
    };
    channel_ = addField("Channel");
    ptt_ = addField("Modem PTT");
    ptt_->SetToolTip("The modem's keying request. Check the radio for actual transmit status.");
    link_ = addField("Link");
    mode_ = addField("Last mode");
    traffic_ = addField("Session traffic");
    burst_ = addField("Last RX burst");
    capture_ = addField("Audio input");
    playback_ = addField("Audio output");
    decode_ = addField("Decoder audio");
    column->Add(fields, 0, wxEXPAND);

    plot_ = new ConstellationPane(panel);
    row->Add(plot_, 1, wxEXPAND);
    column->SetMinSize(FromDIP(wxSize(400, -1)));
    row->Add(column, 0, wxEXPAND | wxLEFT, FromDIP(16));
    auto* attemptsHeader = new wxBoxSizer(wxHORIZONTAL);
    attemptsHeader->Add(new wxStaticText(panel, wxID_ANY, "Receive / decode attempts"),
                        1, wxALIGN_CENTER_VERTICAL);
    clear_ = new wxButton(panel, wxID_ANY, "Clear", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    clear_->Enable(false);
    clear_->SetToolTip("Clear the displayed decode history. The modem continues receiving.");
    attemptsHeader->Add(clear_, 0, wxLEFT, FromDIP(8));
    attempts_ = new wxListCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                               wxLC_REPORT | wxLC_SINGLE_SEL);
    attempts_->SetName("Data2G decode attempts");
    attempts_->AppendColumn("Time (UTC)");
    attempts_->AppendColumn("Mode / codewords");
    attempts_->AppendColumn("SNR (dB)", wxLIST_FORMAT_RIGHT);
    attempts_->AppendColumn("Result");
    attempts_->SetMinSize(FromDIP(wxSize(-1, 150)));
    attempts_->SetToolTip("Newest first; up to 500 detected bursts. Results come from Data2G. "
                          "Undecoded can also mean a burst was not addressed to this station.");
    attempts_->Bind(wxEVT_SIZE, [this](wxSizeEvent& e) { fitColumns(); e.Skip(); });
    clear_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        attempts_->DeleteAllItems();
        clear_->Enable(false);
    });
    auto* contents = new wxBoxSizer(wxVERTICAL);
    contents->Add(row, 1, wxEXPAND);
    contents->Add(attemptsHeader, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(8));
    contents->Add(attempts_, 1, wxEXPAND);
    panel->SetSizer(contents);

    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(panel, 1, wxEXPAND | wxALL, FromDIP(12));
    SetSizerAndFit(root);
    SetMinClientSize(FromDIP(wxSize(700, 490)));
    SetClientSize(FromDIP(wxSize(800, 560)));
    fitColumns();
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { poll(); }, timer_.GetId());
}

void ModemMonitor::setOpen(bool open)
{
    if (open)
    {
        poll();
        Show();
        if (IsIconized()) Iconize(false);
        Raise();
        timer_.Start(POLL_MS);
    }
    else
    {
        timer_.Stop();
        Hide();
    }
}

void ModemMonitor::reset(const wxString& error)
{
    status_->SetToolTip(error);
    update(status_, error.empty() ? wxString("Built-in Data2G stopped") : error);
    for (auto* field : {channel_, ptt_, link_, mode_, traffic_, capture_, playback_, decode_})
        update(field, "—");
    update(burst_, "Waiting for an RX burst");
    plot_->clear();
    if (IsShown()) poll();
}

void ModemMonitor::poll()
{
    const auto state = modem_.telemetry();
    if (!state) return;

    update(status_, wxString::FromUTF8(state->audio));
    update(channel_, state->busy ? "Busy" : "Clear");
    update(ptt_, state->ptt ? "Requested" : "Off");
    update(link_, wxString::FromUTF8(state->link));
    update(mode_, state->mode.empty() ? wxString("—") : wxString::FromUTF8(state->mode));
    update(traffic_, wxString::Format("RX %lld B   TX %lld B",
                                     static_cast<long long>(state->rxBytes), static_cast<long long>(state->txBytes)));
    update(capture_, wxString::Format("%llu overflow · %llu late · %llu dropped · %.1f s backlog",
                                     static_cast<unsigned long long>(state->overflows),
                                     static_cast<unsigned long long>(state->late),
                                     static_cast<unsigned long long>(state->dropped), state->backlogSeconds));
    update(playback_, wxString::Format("%llu underruns", static_cast<unsigned long long>(state->underruns)));
    update(decode_, wxString::Format("%llu samples dropped", static_cast<unsigned long long>(state->decodeDropped)));

    if (auto plot = modem_.takeConstellation()) plot_->setPlot(std::move(*plot));
    appendBursts(modem_.takeBursts());
}

void ModemMonitor::appendBursts(const std::vector<ModemBurst>& bursts)
{
    if (bursts.empty()) return;
    attempts_->Freeze();
    for (const auto& burst : bursts)
    {
        // Match Data2G's native burst log, including its distinction
        // between a lost burst and one that was not decoded for us.
        wxString result = burst.lost ? wxString("Lost")
                        : burst.decoded == 0 ? wxString("Undecoded")
                        : burst.failed ? wxString::Format("Incomplete data (%d/%d)",
                                                           burst.codewords - burst.failed, burst.codewords)
                        : wxString("OK");
        const auto when = wxDateTime(static_cast<time_t>(burst.whenMs / 1000));
        const long row = attempts_->InsertItem(0, when.ToUTC().FormatISOTime());
        attempts_->SetItem(row, 1, wxString::FromUTF8(burst.mode) + wxString::Format(" x%d", burst.codewords));
        attempts_->SetItem(row, 2, burst.snrDb ? wxString::Format("%.1f", *burst.snrDb) : wxString("—"));
        attempts_->SetItem(row, 3, result);
        wxString label = wxString::FromUTF8(burst.mode);
        if (burst.snrDb) label += wxString::Format(" · %.1f dB SNR", *burst.snrDb);
        label += " · " + result;
        update(burst_, label);
    }
    while (attempts_->GetItemCount() > MAX_BURSTS) attempts_->DeleteItem(attempts_->GetItemCount() - 1);
    attempts_->Thaw();
    clear_->Enable(true);
}

void ModemMonitor::fitColumns()
{
    const int width = attempts_->GetClientSize().x;
    if (width <= 0) return;
    const int time = FromDIP(100);
    const int snr = FromDIP(88);
    const int room = std::max(0, width - time - snr - FromDIP(4));
    const int mode = room * 45 / 100;
    attempts_->SetColumnWidth(0, time);
    attempts_->SetColumnWidth(1, mode);
    attempts_->SetColumnWidth(2, snr);
    attempts_->SetColumnWidth(3, room - mode);
}
