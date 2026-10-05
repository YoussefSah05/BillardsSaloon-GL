#include "audio/sound_synth.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace BilliardsSaloon::Audio;

namespace
{
    double rms(const SoundBuffer& sound)
    {
        double sum = 0.0;
        for (const float s : sound.samples)
        {
            sum += static_cast<double>(s) * s;
        }
        return std::sqrt(sum / static_cast<double>(std::max<std::size_t>(sound.samples.size(), 1)));
    }

    // Energy in the first and last fifth of a sound: impacts must die away.
    double tailRatio(const SoundBuffer& sound)
    {
        const std::size_t fifth = sound.samples.size() / 5;
        double head = 0.0;
        double tail = 0.0;
        for (std::size_t i = 0; i < fifth; ++i)
        {
            head += std::abs(sound.samples[i]);
            tail += std::abs(sound.samples[sound.samples.size() - 1 - i]);
        }
        return tail / std::max(head, 1.0e-9);
    }
}

TEST_CASE("synthesised impacts are short, audible, unclipped and decay")
{
    for (const SoundBuffer& sound : {ballClick(1), cushionThud(2), pocketDrop(3), cueStrike(4), uiTick(), uiConfirm()})
    {
        CHECK(sound.seconds() > 0.02);
        CHECK(sound.seconds() < 1.0);
        CHECK(peak(sound) <= 1.0f);
        CHECK(peak(sound) > 0.3f);
        CHECK(tailRatio(sound) < 0.25);
    }
}

TEST_CASE("synthesis is deterministic per seed and varies between seeds")
{
    CHECK(ballClick(5).samples == ballClick(5).samples);
    CHECK(ballClick(5).samples != ballClick(6).samples);
}

TEST_CASE("crowd beds are steady, applause swells and fades")
{
    const SoundBuffer murmur = crowdMurmur(4.0, 11);
    CHECK(murmur.seconds() > 3.5);
    CHECK(peak(murmur) <= 1.0f);
    CHECK(rms(murmur) > 0.02);

    const SoundBuffer clap = applause(3.0, 12);
    CHECK(peak(clap) <= 1.0f);
    CHECK(tailRatio(clap) < 0.6);
}

TEST_CASE("a ball click is brighter than a cushion thud")
{
    // Zero crossings per second as a rough brightness measure.
    const auto crossings = [](const SoundBuffer& sound)
    {
        int count = 0;
        for (std::size_t i = 1; i < sound.samples.size(); ++i)
        {
            count += ((sound.samples[i - 1] < 0.0f) != (sound.samples[i] < 0.0f)) ? 1 : 0;
        }
        return static_cast<double>(count) / sound.seconds();
    };
    CHECK(crossings(ballClick(1)) > 3.0 * crossings(cushionThud(1)));
}

#include "audio/shot_sounds.h"
#include "gameplay/game_variant.h"
#include "gameplay/sim_bridge.h"

#include <algorithm>

TEST_CASE("a break sounds like a break: a hard strike, a loud first contact, then softer clicks and cushions")
{
    using namespace BilliardsSaloon;
    const GameVariantDefinition& variant = eightBallVariant();
    const Sim::Table table = Sim::buildPocketTable(variant.table.pocketGeometry);

    // The rack along the table, the cue ball on the head spot, struck hard at the apex.
    std::vector<Sim::BallState> balls(1);
    balls[0].r = glm::dvec3(0.635, 0.635, variant.table.simBall.R);
    const std::vector<glm::vec3> rack = buildRackPositions(variant);
    for (const glm::vec3& p : rack)
    {
        Sim::BallState ball;
        ball.r = SimBridge::toSimPosition(p, variant.table.clothWidth, variant.table.clothDepth);
        ball.r.z = variant.table.simBall.R;
        balls.push_back(ball);
    }
    Sim::CueStrike strike;
    strike.speed = 7.0;
    strike.phiDegrees = 90.0;
    const Sim::ShotTrajectory shot = Sim::simulateShot(table, balls, 0, strike, variant.table.simBall);

    const std::vector<Audio::SoundCue> cues = Audio::planShotSounds(shot, variant.table.clothWidth, variant.table.clothDepth);
    REQUIRE(cues.size() > 10);
    CHECK(cues.front().kind == Audio::SoundKind::CueStrike);
    CHECK(cues.front().intensity > 0.8f);

    const auto firstContact = std::find_if(cues.begin(), cues.end(), [](const Audio::SoundCue& c) { return c.kind == Audio::SoundKind::BallBall; });
    REQUIRE(firstContact != cues.end());
    CHECK(firstContact->intensity > 0.7f);

    bool cushion = false;
    for (std::size_t i = 1; i < cues.size(); ++i)
    {
        CHECK(cues[i].time >= cues[i - 1].time);
        CHECK(cues[i].intensity <= 1.0f);
        CHECK(cues[i].intensity >= 0.03f);
        cushion = cushion || (cues[i].kind == Audio::SoundKind::Cushion);
    }
    CHECK(cushion);
}
