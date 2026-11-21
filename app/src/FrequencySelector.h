#ifndef RAGTIME_FREQUENCY_SELECTOR_H
#define RAGTIME_FREQUENCY_SELECTOR_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <wx/combobox.h>
#include <wx/timer.h>

class FrequencySelector : public wxComboBox
{
public:
    struct Hooks
    {
        std::function<bool()> available;
        std::function<bool(uint64_t)> tune;
        std::function<std::optional<uint64_t>()> read;
        std::function<std::string()> problem;
        std::function<void()> cancel;
    };

    explicit FrequencySelector(wxWindow* parent);
    void setHooks(Hooks hooks);
    ~FrequencySelector() override;
    void cancel();
    void reset();
    void tune(uint64_t hz) { submit(hz); }

private:
    void reload();
    void tick();
    void selectRemembered();
    void submit(uint64_t hz);
    bool editing() const;
    void finish(const wxString& problem);

    Hooks hooks_;
    wxTimer timer_;
    std::vector<uint64_t> frequencies_;
    std::string fileText_;
    wxString fileProblem_;
    bool dirty_ = false;
    bool updating_ = false;
    uint64_t remembered_ = 7066000;
    uint64_t pending_ = 0;
    uint64_t startedMs_ = 0;
};

#endif
