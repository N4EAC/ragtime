#include "ragtime/audio/EarAudio.h"
#include "CoreAudioCapture.h"
#include "CoreAudioQueue.h"
#include "MicrophonePermission.h"

#import <Foundation/Foundation.h>

#include <atomic>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <thread>

namespace ragtime { namespace audio {
namespace
{

struct EarPermission
{
    std::atomic<bool> pending{false};
    std::atomic<bool> completed{false};
};

std::string signature(const EarStatus& s)
{
    std::ostringstream out;
    out << s.available << '\n' << s.problem << '\n' << s.chosen << '\n'
        << static_cast<int>(s.pick) << ':' << s.listening << ':' << s.streaming << ':' << s.rate
        << '\n' << s.listeningName << '\n' << s.listeningDescription
        << '\n' << s.host.candidates << ':' << s.host.why;
    if (s.host.found)
        out << ':' << s.host.found->pid << ':' << s.host.found->source << ':' << s.host.found->sourceDescription;
    for (const auto& source : s.sources) out << '\n' << source.id << ':' << source.name << ':' << source.description;
    return out.str();
}

class CoreAudioEar : public EarAudio
{
public:
    ~CoreAudioEar() override { stop(); }

    void start(Samples samples, Changed changed) override
    {
        stop();
        samples_ = std::move(samples);
        changed_ = std::move(changed);
        permission_ = std::make_shared<EarPermission>();
        lastHostSource_.clear();
        lastSignature_.clear();
        {
            std::lock_guard<std::mutex> lock(controlMutex_);
            quit_ = false;
            dirty_ = true;
        }
        worker_ = std::thread([this] { run(); });
    }

    void stop() override
    {
        {
            std::lock_guard<std::mutex> lock(controlMutex_);
            quit_ = true;
        }
        wake_.notify_one();
        if (worker_.joinable()) worker_.join();
        samples_ = nullptr;
        changed_ = nullptr;
        std::lock_guard<std::mutex> lock(statusMutex_);
        published_ = {};
    }

    void choose(const std::string& source) override
    {
        {
            std::lock_guard<std::mutex> lock(controlMutex_);
            if (chosen_ == source) return;
            chosen_ = source;
            dirty_ = true;
        }
        wake_.notify_one();
    }

    void setHostPid(std::optional<int> pid) override
    {
        {
            std::lock_guard<std::mutex> lock(controlMutex_);
            if (hostPid_ == pid) return;
            hostPid_ = pid;
            dirty_ = true;
        }
        wake_.notify_one();
    }

    EarStatus status() const override
    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        return published_;
    }

private:
    bool authorized(EarStatus& state)
    {
        const auto access = macos::microphonePermission();
        if (access == macos::MicrophonePermission::Authorized) return true;
        if (access == macos::MicrophonePermission::Denied)
            state.problem = "Audio input access is denied; enable Ragtime in System Settings > Privacy & Security > Microphone";
        else
        {
            auto permission = permission_;
            if (!permission->pending.exchange(true) &&
                !macos::requestMicrophonePermission([permission] { permission->completed = true; }))
            {
                permission->pending = false;
                state.problem = "Audio input permission must be requested from the Ragtime app bundle";
            }
            else state.problem = "Waiting for microphone permission to listen to the audio input";
        }
        return false;
    }

    void evaluate(EarStatus& state, const macos::DeviceSnapshot& snapshot,
                  const std::string& chosen, std::optional<int> pid)
    {
        state = {};
        state.available = snapshot.problem.empty();
        state.problem = snapshot.problem;
        state.chosen = chosen;
        state.following = chosen.empty();
        const auto graph = macos::deviceGraph(snapshot);
        state.sources = ear::sources(graph);
        state.host = ear::findHostCapture(graph, pid);
        // A process can open several inputs, including an embedded GUI
        // hosting several modems. Its PID alone cannot select among them.
        if (state.host.found)
        {
            int captures = 0;
            for (const auto& node : graph.nodes)
                if (node.mediaClass == "Stream/Input/Audio")
                    if (const auto* client = graph.client(node.clientId))
                        if (client->pid == state.host.found->pid) ++captures;
            if (captures > 1)
            {
                state.host.found.reset();
                state.host.why = "data2g-host has several audio inputs; pick a source in Listen to";
            }
        }
        if (state.host.found && state.host.found->source)
            lastHostSource_ = state.host.found->sourceName;
        const bool ambiguous = state.host.candidates > 1 && !state.host.found;
        const auto pick = ambiguous && chosen.empty() ? ear::SourcePick{}
            : ear::pickSource(graph, chosen, state.host, lastHostSource_, macos::defaultName(snapshot, true));
        const auto* device = pick.node ? macos::findDevice(snapshot, pick.node->name, true) : nullptr;
        if (capture_ && (!device || captureName_ != pick.node->name ||
                         std::fabs(device->rate - capture_->rate()) > 0.5))
            capture_.reset();
        if (!state.available) return;
        if (ambiguous && chosen.empty())
        {
            state.problem = "Several local host captures are possible; select the radio input in Listen to";
            return;
        }
        if (!device)
        {
            state.problem = chosen.empty() ? "No audio input is available" : "The selected audio input is not connected";
            return;
        }
        state.pick = pick.why;
        state.listeningName = pick.node->name;
        state.listeningDescription = pick.node->description.empty() ? pick.node->name : pick.node->description;
        if (!authorized(state))
        {
            capture_.reset();
            return;
        }
        if (!samples_) return;
        if (!capture_)
        {
            capture_ = macos::openInputCapture(*device, 100,
                [receive = samples_](const float* samples, std::size_t n, int rate) {
                    receive(samples, n, rate, std::nullopt);
                }, state.problem);
            captureName_ = capture_ ? pick.node->name : "";
        }
        if (capture_)
        {
            state.listening = pick.node->id;
            state.rate = capture_->rate();
        }
    }

    void publish(const EarStatus& state)
    {
        const auto key = signature(state);
        if (key == lastSignature_) return;
        lastSignature_ = key;
        {
            std::lock_guard<std::mutex> lock(statusMutex_);
            published_ = state;
        }
        if (changed_) changed_();
    }

    void run()
    {
        auto nextScan = std::chrono::steady_clock::now();
        EarStatus state;
        for (;;)
        {
            @autoreleasepool
            {
                bool scan;
                std::string chosen;
                std::optional<int> pid;
                {
                    std::unique_lock<std::mutex> lock(controlMutex_);
                    wake_.wait_for(lock, std::chrono::milliseconds(20), [this] { return quit_ || dirty_; });
                    if (quit_) break;
                    scan = dirty_;
                    dirty_ = false;
                }
                scan = permission_->completed.exchange(false) || scan || std::chrono::steady_clock::now() >= nextScan;
                if (scan)
                {
                    const auto snapshot = macos::readDevices();
                    // A selection may have changed while native discovery
                    // was running. Use the most recent one before opening.
                    {
                        std::lock_guard<std::mutex> lock(controlMutex_);
                        if (quit_) break;
                        chosen = chosen_;
                        pid = hostPid_;
                    }
                    evaluate(state, snapshot, chosen, pid);
                    nextScan = std::chrono::steady_clock::now() + std::chrono::milliseconds(750);
                }
                if (capture_)
                {
                    const auto problem = capture_->problem();
                    if (!problem.empty() || macos::microphonePermission() != macos::MicrophonePermission::Authorized)
                    {
                        state.problem = problem.empty() ? "Audio input permission was revoked" : problem;
                        capture_.reset();
                        state.listening = 0;
                        state.rate = 0;
                    }
                }
                const auto last = capture_ ? capture_->lastSamplesMs() : 0;
                state.streaming = last && macos::audioNowMs() - last < 1000;
                publish(state);
            }
        }
        capture_.reset();
    }

    mutable std::mutex statusMutex_;
    EarStatus published_;
    std::mutex controlMutex_;
    std::condition_variable wake_;
    bool quit_ = false;
    bool dirty_ = true;
    std::string chosen_;
    std::optional<int> hostPid_;
    std::thread worker_;
    Samples samples_;
    Changed changed_;
    std::shared_ptr<EarPermission> permission_;
    std::unique_ptr<macos::InputCapture> capture_;
    std::string captureName_;
    std::string lastHostSource_;
    std::string lastSignature_;
};

} // namespace

std::unique_ptr<EarAudio> makeEarAudio()
{
    return std::make_unique<CoreAudioEar>();
}

} } // namespace ragtime::audio
