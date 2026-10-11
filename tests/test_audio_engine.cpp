#include "app/audio_engine.h"
#include "core/asset_paths.h"
#include <doctest/doctest.h>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <vector>

using namespace Septhuit;

namespace
{
    float peak(const std::vector<float>& samples)
    {
        float result = 0.0f;
        for (const float sample : samples) result = std::max(result, std::abs(sample));
        return result;
    }
    std::vector<float> render(AudioEngine& engine, double seconds)
    {
        std::vector<float> samples(static_cast<std::size_t>(seconds * Audio::SAMPLE_RATE) * 2);
        REQUIRE(engine.renderOffline(samples));
        return samples;
    }
}

TEST_CASE("bundled recorded bank covers every event and decodes without clipping")
{
    const auto samples = Audio::loadRecordedBank(resolveAssetPath("audio/bank.json"));
    REQUIRE(samples.size() == 48);
    std::set<std::string> banks;
    std::set<std::pair<std::string, int>> layers;
    for (const auto& sample : samples)
    {
        INFO(sample.file);
        REQUIRE_FALSE(sample.pcm.samples.empty());
        CHECK(sample.pcm.seconds() <= 20.0);
        CHECK(peak(sample.pcm.samples) > 0.3f);
        CHECK(peak(sample.pcm.samples) <= 0.83f);
        CHECK(std::all_of(sample.pcm.samples.begin(), sample.pcm.samples.end(),
                          [](float value) { return std::isfinite(value); }));
        banks.insert(sample.bank);
        layers.emplace(sample.bank, sample.strength);
        if (sample.bank == "roll" || sample.bank == "murmur")
        {
            CHECK(sample.pcm.samples.front() == sample.pcm.samples.back());
        }
        else
        {
            // Edited one-shots finish at zero to avoid truncation clicks.
            CHECK(sample.pcm.samples.back() == 0.0f);
        }
    }
    CHECK(banks.size() == 16);
    for (const std::string bank : {"ball", "cushion", "cue"})
    {
        for (int strength = 0; strength < 3; ++strength)
        {
            CHECK(layers.contains({bank, strength}));
        }
    }
    CHECK_THROWS(static_cast<void>(Audio::loadRecordedBank(resolveAssetPath("audio/bank.json").parent_path() / "missing.json")));
}

TEST_CASE("offline audio schedules contacts, reuses voices and stops pending effects")
{
    AudioEngine engine(true, true);
    REQUIRE(engine.available());
    engine.setVolumes(1.0f, 1.0f, 0.0f);
    const Audio::SoundCue impact{0.0, Audio::SoundKind::BallBall, 1.0f, {0.0f, 0.0f, 0.0f}};
    engine.playShotCue(impact, 0.05);
    CHECK(peak(render(engine, 0.04)) == 0.0f);
    CHECK(peak(render(engine, 0.08)) > 0.1f);
    render(engine, 0.1);
    engine.update();
    engine.playShotCue(impact);
    CHECK(peak(render(engine, 0.12)) > 0.1f);
    for (int i = 0; i < 200; ++i) engine.playShotCue(impact);
    const auto busy = render(engine, 0.15);
    CHECK(peak(busy) > 0.1f);
    CHECK(peak(busy) <= 0.9201f);
    for (const float sample : busy) REQUIRE(std::isfinite(sample));
    engine.playShotCue(impact, 0.05);
    engine.stopEffects();
    // Already mixed frames cannot be recalled from the engine/device buffer.
    // They may contain the preceding overlap, but the scheduled contact must
    // never reach the output after that short buffer has drained.
    render(engine, 0.01);
    CHECK(peak(render(engine, 0.15)) == 0.0f);
}

TEST_CASE("effects volume mutes gameplay and rolling follows ball motion")
{
    AudioEngine engine(true, true);
    REQUIRE(engine.available());
    engine.setVolumes(1.0f, 0.0f, 0.0f);
    engine.playGameplay(GameplaySound::Foul);
    engine.playGameplay(GameplaySound::BallPlace);
    engine.setRolling(2.0f, glm::vec3(0.0f));
    render(engine, 0.02); // Allow miniaudio's volume smoothing to settle.
    CHECK(peak(render(engine, 0.1)) == 0.0f);
    engine.stopEffects(); // Isolate rolling from the notification's remaining tail.
    engine.setVolumes(1.0f, 1.0f, 0.0f);
    engine.setRolling(2.0f, glm::vec3(0.0f));
    CHECK(peak(render(engine, 0.1)) > 0.001f);
    engine.setRolling(0.0f, glm::vec3(0.0f));
    render(engine, 0.02);
    CHECK(peak(render(engine, 0.1)) == 0.0f);
    engine.stopEffects();
    engine.playGameplay(GameplaySound::Rack);
    CHECK(peak(render(engine, 0.8)) > 0.01f);
}

TEST_CASE("camera relative impacts reach the expected stereo side")
{
    AudioEngine engine(true, true);
    REQUIRE(engine.available());
    engine.setListener({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    engine.playShotCue({0.0, Audio::SoundKind::BallBall, 1.0f, {2.0f, 0.0f, 0.0f}});
    const auto samples = render(engine, 0.15);
    double left = 0.0;
    double right = 0.0;
    for (std::size_t i = 0; i < samples.size(); i += 2)
    {
        left += static_cast<double>(samples[i]) * samples[i];
        right += static_cast<double>(samples[i + 1]) * samples[i + 1];
    }
    CHECK(right > 3.0 * left);
}
