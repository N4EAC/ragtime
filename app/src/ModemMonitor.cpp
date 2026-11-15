#include "ModemMonitor.h"

#include <utility>

#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "ConstellationPane.h"
#include "EmbeddedModem.h"

namespace
{
constexpr int POLL_MS = 150;

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
    decode_ = addField("Decoder");
    column->Add(fields, 0, wxEXPAND);

    plot_ = new ConstellationPane(panel);
    row->Add(plot_, 1, wxEXPAND);
    column->SetMinSize(FromDIP(wxSize(400, -1)));
    row->Add(column, 0, wxEXPAND | wxLEFT, FromDIP(16));
    panel->SetSizer(row);

    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(panel, 1, wxEXPAND | wxALL, FromDIP(12));
    SetSizerAndFit(root);
    SetClientSize(FromDIP(wxSize(760, 350)));
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
    if (const auto burst = modem_.takeLatestBurst())
    {
        wxString label = wxString::FromUTF8(burst->mode);
        if (burst->snrDb) label += wxString::Format(" · %.1f dB SNR", *burst->snrDb);
        if (burst->lost) label += " · Lost";
        update(burst_, label);
    }
}
