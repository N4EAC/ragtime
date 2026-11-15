#include <algorithm>
#include <cmath>
#include <vector>

#include "Check.h"
#include "ragtime/audio/EarAudio.h"
#include "ragtime/ear/Spectrum.h"

int main()
{
    constexpr int rate = 8000;
    constexpr double frequency = 1000;
    constexpr double pi = 3.14159265358979323846;
    std::vector<float> signal(rate);
    for (size_t i = 0; i < signal.size(); ++i)
        signal[i] = 0.2f * std::sin(2 * pi * frequency * i / rate);

    auto ear = ragtime::audio::makeEarAudio();
    // Select the modem before startup so the test needs no microphone.
    ear->hostAudio("TCI receive audio", nullptr, 0, rate);
    ragtime::ear::Spectrum spectrum(rate, 10);
    std::vector<float> latest;
    size_t received = 0;
    size_t transmitted = 0;
    ear->start(
        [&](const float* samples, size_t n, int sampleRate, std::optional<bool> tx) {
            CHECK_EQ(sampleRate, rate);
            CHECK(tx.has_value());
            if (tx.value_or(false)) transmitted += n;
            else received += n;
            for (auto& row : spectrum.push(samples, n)) latest = std::move(row);
        },
        nullptr);
    CHECK(ear->status().hostTap);
    CHECK(!ear->status().streaming);

    ear->hostAudio("TCI receive audio", signal.data(), signal.size(), rate);
    CHECK_EQ(received, signal.size());
    CHECK(ear->status().streaming);
    CHECK_EQ(ear->status().rate, rate);
    CHECK(!latest.empty());
    if (!latest.empty())
    {
        const auto peak = std::max_element(latest.begin(), latest.end()) - latest.begin();
        CHECK(std::abs(ragtime::ear::Spectrum::binHz(peak, spectrum.topHz()) - frequency) < 25);
    }

    // An explicit input bypasses the modem even when that input is gone.
    ear->choose("ragtime-test-missing-input");
    ear->hostAudio("TCI receive audio", signal.data(), signal.size(), rate);
    CHECK(!ear->status().hostTap);
    CHECK(!ear->status().streaming);
    CHECK_EQ(received, signal.size());

    ear->choose("");
    ear->hostAudio("TCI receive audio", signal.data(), signal.size(), rate);
    CHECK(ear->status().hostTap);
    CHECK_EQ(received, signal.size() * 2);

    ear->hostAudio("TCI receive audio", signal.data(), signal.size(), rate, true);
    CHECK_EQ(transmitted, signal.size());
    CHECK_EQ(received, signal.size() * 2);

    ear->stop();
    ear->hostAudio("TCI receive audio", signal.data(), signal.size(), rate);
    CHECK(!ear->status().streaming);
    CHECK_EQ(received, signal.size() * 2);
    return ragtime_test::finish();
}
