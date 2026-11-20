// The real Ear worker, with fake discovery, permission and capture. No
// physical device is opened, even when capture succeeds in these tests.
#include "Check.h"
#include "CoreAudioCapture.h"
#include "CoreAudioQueue.h"
#include "MicrophonePermission.h"
#include "ragtime/audio/EarAudio.h"

#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

using namespace ragtime;
namespace mac = audio::macos;

namespace
{

std::mutex mutex;
mac::DeviceSnapshot snapshot;
std::string openedUid;
std::function<void()> permissionCompleted;
std::atomic<mac::MicrophonePermission> permission{mac::MicrophonePermission::Authorized};
std::atomic<int> opened{0}, closed{0}, active{0}, maxActive{0}, requests{0};
std::atomic<unsigned> bufferMs{0};
std::atomic<bool> failCapture{false}, failOpen{false}, allowRequest{true};

class FakeCapture : public mac::InputCapture
{
public:
    FakeCapture(int rate, mac::CaptureSamples samples) : rate_(rate)
    {
        const int running = ++active;
        maxActive = std::max(maxActive.load(), running);
        ++opened;
        thread_ = std::thread([this, samples = std::move(samples)] {
            const std::array<float, 64> chunk{};
            while (!quit_)
            {
                samples(chunk.data(), chunk.size(), rate_);
                last_ = mac::audioNowMs();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }
    ~FakeCapture() override
    {
        quit_ = true;
        thread_.join();
        --active;
        ++closed;
    }
    int rate() const override { return rate_; }
    int64_t lastSamplesMs() const override { return last_; }
    std::string problem() const override { return failCapture ? "test input stopped" : ""; }

private:
    int rate_;
    std::atomic<bool> quit_{false};
    std::atomic<int64_t> last_{0};
    std::thread thread_;
};

template <typename Predicate>
bool waitFor(Predicate predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!predicate())
    {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}

mac::DeviceSnapshot station()
{
    mac::DeviceSnapshot s;
    s.devices = {{10, "built-in", "Built in", 1, 2, 48000, {}},
                 {20, "radio-a", "Radio A", 2, 2, 44100, {}},
                 {30, "radio-b", "Radio B", 1, 2, 48000, {}}};
    s.defaultInput = 10;
    s.processes = {{123, "data2g-host", {20}, {20}}};
    return s;
}

void replace(mac::DeviceSnapshot s)
{
    std::lock_guard<std::mutex> lock(mutex);
    snapshot = std::move(s);
}

} // namespace

namespace ragtime { namespace audio { namespace macos {

DeviceSnapshot readDevices()
{
    std::lock_guard<std::mutex> lock(mutex);
    return snapshot;
}
MicrophonePermission microphonePermission() { return permission; }
bool requestMicrophonePermission(std::function<void()> completed)
{
    ++requests;
    if (!allowRequest) return false;
    std::lock_guard<std::mutex> lock(mutex);
    permissionCompleted = std::move(completed);
    return true;
}
std::unique_ptr<InputCapture> openInputCapture(const Device& d, unsigned ms, CaptureSamples samples, std::string& problem)
{
    std::lock_guard<std::mutex> lock(mutex);
    openedUid = d.uid;
    bufferMs = ms;
    if (failOpen)
    {
        problem = "test exclusive input";
        return nullptr;
    }
    problem.clear();
    return std::make_unique<FakeCapture>(static_cast<int>(d.rate), std::move(samples));
}

} } }

int main()
{
    auto s = station();
    replace(s);
    std::atomic<int> samples{0}, changed{0}, rate{0};
    const auto receive = [&](const float*, std::size_t n, int r) { samples += static_cast<int>(n); rate = r; };
    auto ear = audio::makeEarAudio();
    ear->setHostPid(123); // works before start, as the command-line probe uses it
    ear->start(receive, [&] { ++changed; });
    CHECK(waitFor([&] { return ear->status().streaming; }));
    CHECK(ear->status().pick == ear::Pick::Host);
    CHECK_EQ(ear->status().listeningName, mac::deviceName("radio-a", true));
    CHECK_EQ(rate.load(), 44100);
    CHECK_EQ(bufferMs.load(), 100u);

    s.processes[0].inputs = {30};
    s.processes[0].outputs = {30};
    replace(s);
    CHECK(waitFor([&] { return ear->status().listeningName == mac::deviceName("radio-b", true); }));
    s.processes.clear();
    replace(s);
    CHECK(waitFor([&] { return ear->status().pick == ear::Pick::HostBefore; }));
    CHECK_EQ(ear->status().listeningName, mac::deviceName("radio-b", true));

    ear->choose(mac::deviceName("built-in", true));
    CHECK(waitFor([&] { return ear->status().pick == ear::Pick::Chosen && ear->status().streaming; }));
    CHECK(!ear->status().following);
    CHECK_EQ(ear->status().listeningName, mac::deviceName("built-in", true));
    s.devices.erase(s.devices.begin());
    replace(s);
    CHECK(waitFor([&] { return !ear->status().listening && ear->status().problem.find("not connected") != std::string::npos; }));
    CHECK_EQ(active.load(), 0); // an explicit missing pick never falls back
    s.devices.push_back({999, "built-in", "Built in again", 1, 2, 48000, {}});
    replace(s);
    CHECK(waitFor([&] { return ear->status().streaming; }));
    CHECK_EQ(ear->status().listeningDescription, std::string("Built in again"));
    s.devices.back().rate = 44100;
    replace(s);
    CHECK(waitFor([&] { return ear->status().rate == 44100; }));

    s.problem = "ownership unreadable";
    replace(s);
    CHECK(waitFor([&] { return !ear->status().available; }));
    CHECK_EQ(active.load(), 0);
    s.problem.clear();
    replace(s);
    CHECK(waitFor([&] { return ear->status().streaming; }));
    failCapture = true;
    CHECK(waitFor([&] { return ear->status().problem == "test input stopped"; }));
    CHECK(!ear->status().streaming);
    failCapture = false;
    CHECK(waitFor([&] { return ear->status().streaming; }));
    permission = mac::MicrophonePermission::Denied;
    CHECK(waitFor([&] { return !ear->status().listening; }));
    CHECK_EQ(active.load(), 0);
    permission = mac::MicrophonePermission::Authorized;
    CHECK(waitFor([&] { return ear->status().streaming; }));

    s.processes = {{1, "data2g-host", {20}, {}}, {2, "data2g-gui", {30}, {}}};
    replace(s);
    ear->choose("");
    CHECK(waitFor([&] { return ear->status().host.candidates == 2; }));
    CHECK(!ear->status().listening); // no automatic fallback amid ambiguity
    CHECK(!ear->status().host.found);
    ear->setHostPid(2);
    CHECK(waitFor([&] { return ear->status().streaming; }));
    CHECK_EQ(ear->status().listeningName, mac::deviceName("radio-b", true));
    s.processes = {{2, "data2g-gui", {20, 30}, {}}};
    replace(s);
    CHECK(waitFor([&] { return !ear->status().listening; }));
    CHECK(!ear->status().host.found); // one PID is not enough for two inputs
    ear->choose(mac::deviceName("radio-a", true));
    CHECK(waitFor([&] { return ear->status().streaming; }));
    CHECK_EQ(ear->status().sources.size(), 3u); // Ear offers the radio inputs
    ear->stop();
    const int stoppedSamples = samples, stoppedChanges = changed;
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    CHECK_EQ(samples.load(), stoppedSamples);
    CHECK_EQ(changed.load(), stoppedChanges);
    CHECK_EQ(active.load(), 0);
    CHECK(!ear->status().available);
    CHECK_EQ(maxActive.load(), 1); // old queue closes before its replacement

    replace(station());
    permission = mac::MicrophonePermission::NotDetermined;
    ear->choose("missing");
    ear->start(receive, [&] { ++changed; });
    CHECK(waitFor([&] { return ear->status().available; }));
    CHECK_EQ(requests.load(), 0); // no prompt when no selected device exists
    ear->choose(mac::deviceName("radio-a", true));
    CHECK(waitFor([&] { return ear->status().problem.find("Waiting") != std::string::npos; }));
    CHECK_EQ(active.load(), 0);
    CHECK_EQ(requests.load(), 1);
    std::function<void()> late;
    {
        std::lock_guard<std::mutex> lock(mutex);
        late = permissionCompleted;
    }
    permission = mac::MicrophonePermission::Authorized;
    late();
    CHECK(waitFor([&] { return ear->status().streaming; }));
    ear->stop();
    permission = mac::MicrophonePermission::Denied;
    ear->start(receive, nullptr);
    CHECK(waitFor([&] { return ear->status().problem.find("denied") != std::string::npos; }));
    CHECK_EQ(active.load(), 0);
    ear->stop();
    permission = mac::MicrophonePermission::NotDetermined;
    allowRequest = false;
    ear->start(receive, nullptr);
    CHECK(waitFor([&] { return ear->status().problem.find("app bundle") != std::string::npos; }));
    ear.reset();
    late(); // native permission replies can arrive after the window is gone
    auto noHost = station();
    noHost.processes.clear();
    replace(noHost);
    permission = mac::MicrophonePermission::Authorized;
    ear = audio::makeEarAudio();
    failOpen = true;
    ear->start(receive, nullptr);
    CHECK(waitFor([&] { return ear->status().problem == "test exclusive input"; }));
    CHECK(ear->status().available);
    CHECK_EQ(active.load(), 0);
    failOpen = false;
    CHECK(waitFor([&] { return ear->status().streaming; }));
    CHECK(ear->status().pick == ear::Pick::Default);
    CHECK_EQ(ear->status().listeningName, mac::deviceName("built-in", true));
    ear->stop();
    CHECK_EQ(active.load(), 0);
    CHECK_EQ(opened.load(), closed.load());
    return ragtime_test::finish();
}
