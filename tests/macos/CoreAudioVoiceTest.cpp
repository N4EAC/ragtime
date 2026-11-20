// A deliberately unavailable/radio-only inventory. Exercise the actual
// backend's command and shutdown paths without opening physical devices
// or asking for microphone permission in the test suite.
#include "Check.h"
#include "CoreAudioDevices.h"
#include "MicrophonePermission.h"
#include "ragtime/audio/VoiceAudio.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>

namespace
{
std::atomic<bool> offerMicrophone{false}, allowRequest{true};
std::atomic<ragtime::audio::macos::MicrophonePermission> permission{
    ragtime::audio::macos::MicrophonePermission::NotDetermined};
std::atomic<int> requests{0};
std::mutex permissionMutex;
std::function<void()> permissionCompleted;
}

namespace ragtime { namespace audio { namespace macos {

DeviceSnapshot readDevices()
{
    DeviceSnapshot s;
    s.devices = {{10, "test-radio", "Test radio", 1, 2, 48000, {}}};
    s.processes = {{123, "data2g-host", {10}, {10}}};
    s.defaultInput = s.defaultOutput = 10;
    if (offerMicrophone)
    {
        // A deliberately unreadable sample rate stops before queue creation,
        // even in the grant test. No test may open the physical default mic.
        s.devices.push_back({11, "test-microphone", "Test microphone", 1, 0, 0, {}});
        s.defaultInput = 11;
    }
    return s;
}

MicrophonePermission microphonePermission() { return permission; }
bool requestMicrophonePermission(std::function<void()> completed)
{
    ++requests;
    if (!allowRequest) return false;
    std::lock_guard<std::mutex> lock(permissionMutex);
    permissionCompleted = std::move(completed);
    return true;
}

} } }

namespace
{

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

} // namespace

int main()
{
    using namespace ragtime;
    auto a = audio::makeVoiceAudio();
    std::atomic<int> finished{0}, played{0}, changed{0}, received{0};
    const auto done = [&](bool heard) { if (heard) ++played; ++finished; };
    a->play({1}, 8000, done); // before start
    CHECK_EQ(finished.load(), 1);
    a->start([&] { ++changed; }, {});
    CHECK(waitFor([&] { return a->status().available; }));
    auto s = a->status();
    CHECK(s.devices.microphones.empty());
    CHECK(s.devices.speakers.empty());
    CHECK_EQ(s.devices.radio.size(), 2u);
    a->chooseMicrophone(audio::macos::deviceName("test-radio", true));
    a->chooseSpeaker(audio::macos::deviceName("test-radio", false));
    CHECK(waitFor([&] { return a->status().speaker.why == voice::Pick::Refused; }));
    a->listen([&](const float*, std::size_t, int) { ++received; });
    CHECK(waitFor([&] { return !a->status().microphoneProblem.empty(); }));
    a->stopListening();
    CHECK_EQ(received.load(), 0);
    a->play({0.1f}, 8000, done); // radio refusal
    CHECK(waitFor([&] { return finished == 2; }));
    a->play({}, 8000, done); // invalid clip
    CHECK(waitFor([&] { return finished == 3; }));
    a->playStream(0, done); // invalid rate
    CHECK(waitFor([&] { return finished == 4; }));
    a->chooseSpeaker("unplugged");
    CHECK(waitFor([&] { return a->status().speaker.why == voice::Pick::Missing; }));
    a->playStream(8000, done);
    a->append(nullptr, 0);
    a->endStream();
    a->stopPlaying();
    CHECK(waitFor([&] { return finished == 5; }));
    // Repeated pending replacements and stop must complete each request
    // exactly once, including commands still queued when stop begins.
    for (int i = 0; i < 50; ++i) a->playStream(8000, done);
    a->stop();
    CHECK_EQ(finished.load(), 55);
    CHECK_EQ(played.load(), 0);
    CHECK(!a->status().playing);
    CHECK(!a->status().listening);
    const auto changes = changed.load();
    a->chooseSpeaker("");
    a->stopListening();
    CHECK_EQ(changed.load(), changes);
    a->play({1}, 8000, done);
    CHECK_EQ(finished.load(), 56);
    a->start([&] { ++changed; }, {});
    CHECK(waitFor([&] { return a->status().available; }));
    a->stop();
    CHECK(!a->status().available);

    offerMicrophone = true;
    a->start([&] { ++changed; }, {});
    a->listen([&](const float*, std::size_t, int) { ++received; });
    CHECK(waitFor([&] { return a->status().microphonePermissionPending; }));
    CHECK(a->status().microphoneProblem.empty());
    CHECK(!a->status().listening);
    CHECK_EQ(requests.load(), 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    CHECK_EQ(requests.load(), 1); // one OS prompt, not one per pump
    std::function<void()> lateCompletion;
    {
        std::lock_guard<std::mutex> lock(permissionMutex);
        lateCompletion = permissionCompleted;
    }
    permission = audio::macos::MicrophonePermission::Denied;
    lateCompletion();
    CHECK(waitFor([&] { return a->status().microphoneProblem.find("denied") != std::string::npos; }));
    CHECK(!a->status().microphonePermissionPending);
    a->stopListening();
    a->stop();
    CHECK(!a->status().microphonePermissionPending);
    const int stoppedChanges = changed;
    lateCompletion(); // an OS response after stop cannot invoke the UI
    CHECK_EQ(changed.load(), stoppedChanges);

    permission = audio::macos::MicrophonePermission::Authorized;
    a->start(nullptr, {});
    a->listen([&](const float*, std::size_t, int) { ++received; });
    CHECK(waitFor([&] { return a->status().microphoneProblem.find("sample rate") != std::string::npos; }));
    CHECK(!a->status().microphonePermissionPending);
    CHECK_EQ(received.load(), 0);
    a->stop();

    permission = audio::macos::MicrophonePermission::NotDetermined;
    allowRequest = false;
    a->start(nullptr, {});
    a->listen([](const float*, std::size_t, int) {});
    CHECK(waitFor([&] { return a->status().microphoneProblem.find("app bundle") != std::string::npos; }));
    CHECK(!a->status().microphonePermissionPending);
    a.reset();
    lateCompletion(); // safe after destruction as well
    return ragtime_test::finish();
}
