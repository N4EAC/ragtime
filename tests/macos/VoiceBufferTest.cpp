#include "Check.h"
#include "VoiceBuffer.h"

#include <thread>

using ragtime::audio::macos::VoiceBuffer;

int main()
{
    float out[4] = {};
    VoiceBuffer clip({1, 2, 3, 4, 5}, false);
    CHECK_EQ(clip.fill(out, 4), 4u);
    CHECK_EQ(out[3], 4.0f);
    CHECK(!clip.eof);
    CHECK_EQ(clip.fill(out, 4), 1u);
    CHECK_EQ(out[0], 5.0f);
    CHECK(clip.eof);
    CHECK_EQ(clip.position.load(), 5u);
    CHECK_EQ(clip.fill(out, 4), 0u);

    VoiceBuffer stream({}, true);
    CHECK_EQ(stream.fill(out, 4), 4u);
    CHECK_EQ(out[0], 0.0f);
    CHECK_EQ(stream.position.load(), 0u);
    CHECK(!stream.eof);
    const float piece[] = {0.25f, 0.5f};
    stream.append(piece, 2);
    CHECK_EQ(stream.fill(out, 4), 4u);
    CHECK_EQ(out[0], piece[0]);
    CHECK_EQ(out[1], piece[1]);
    CHECK_EQ(out[2], 0.0f);
    CHECK_EQ(stream.position.load(), 2u);
    stream.append(piece, 2); // compacts already consumed frames
    stream.end();
    stream.append(piece, 2); // late arrivals cannot extend a closed clip
    CHECK_EQ(stream.fill(out, 4), 2u);
    CHECK_EQ(out[1], piece[1]);
    CHECK(stream.eof);
    CHECK_EQ(stream.position.load(), 4u);

    VoiceBuffer empty({}, true);
    empty.end();
    CHECK_EQ(empty.fill(out, 4), 0u);
    CHECK(empty.eof);

    VoiceBuffer volume({0.25f, 0.5f, 0.25f, 0.5f, 0.25f}, false);
    CHECK_EQ(volume.fill(out, 2, 2.0f), 2u);
    CHECK_EQ(out[0], 0.5f);
    CHECK_EQ(out[1], 1.0f);
    CHECK_EQ(volume.fill(out, 2, 0.5f), 2u);
    CHECK_EQ(out[1], 0.25f);
    CHECK_EQ(volume.fill(out, 2, 0.0f), 1u);
    CHECK_EQ(out[0], 0.0f);
    CHECK_EQ(volume.position.load(), 5u);

    // Producer and callback contend while chunks arrive. Every source
    // frame must be consumed once; silence must not add to the playhead.
    VoiceBuffer concurrent({}, true);
    std::thread producer([&] {
        for (int i = 0; i < 10000; ++i) concurrent.append(piece, 2);
        concurrent.end();
    });
    while (!concurrent.eof) concurrent.fill(out, 4);
    producer.join();
    CHECK_EQ(concurrent.position.load(), 20000u);
    return ragtime_test::finish();
}
