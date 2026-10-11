#include <doctest/doctest.h>
#include <cmath>

#include "audio/shot_sounds.h"
#include "gameplay/game_variant.h"
#include "gameplay/sim_bridge.h"

#include <algorithm>
#include <initializer_list>

using namespace BilliardsSaloon::Audio;

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

    const std::vector<Audio::SoundCue> cues = Audio::planShotSounds(shot, table);
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

TEST_CASE("glancing cushion contacts follow normal impact speed")
{
    using namespace BilliardsSaloon;
    Sim::Table table;
    table.length = 2.54;
    table.width = 1.27;
    table.linear.push_back({{0.0, 0.0, 0.03}, {0.0, 2.54, 0.03}, 0.001});
    Sim::ShotTrajectory shot;
    Sim::BallState ball;
    ball.r = {0.1, 0.5, shot.params.R};
    ball.v = {-0.1, 3.0, 0.0};
    shot.events = {{Sim::EventType::Strike, 0.0, 0}, {Sim::EventType::LinearCushion, 0.0, 0, 0}};
    shot.states = {{ball}, {ball}};
    const auto glancing = planShotSounds(shot, table);
    REQUIRE(glancing.size() == 2);
    CHECK(glancing.back().intensity == doctest::Approx(std::sqrt(0.1 / 4.0)));
    ball.v = {-3.0, 0.1, 0.0};
    shot.states = {{ball}, {ball}};
    const auto headOn = planShotSounds(shot, table);
    REQUIRE(headOn.size() == 2);
    CHECK(headOn.back().intensity > 4.0f * glancing.back().intensity);
}

TEST_CASE("different simultaneous rack contacts survive while repeated pair chatter merges")
{
    using namespace BilliardsSaloon;
    const auto table = Sim::buildPocketTable(Sim::PocketTableSpec{});
    Sim::ShotTrajectory shot;
    std::vector<Sim::BallState> balls(4);
    balls[0].r = {0.2, 0.5, shot.params.R};
    balls[1].r = {0.2 + 2.0 * shot.params.R, 0.5, shot.params.R};
    balls[2].r = {0.7, 1.5, shot.params.R};
    balls[3].r = {0.7 + 2.0 * shot.params.R, 1.5, shot.params.R};
    balls[0].v = balls[2].v = {2.0, 0.0, 0.0};
    shot.events = {{Sim::EventType::Strike, 0.0, 0}, {Sim::EventType::BallBall, 0.0, 0, 1},
                   {Sim::EventType::BallBall, 0.0, 2, 3}, {Sim::EventType::BallBall, 0.001, 0, 1}};
    shot.states.assign(shot.events.size(), balls);
    const auto cues = planShotSounds(shot, table);
    REQUIRE(cues.size() == 3);
    CHECK(cues[1].ball == 0);
    CHECK(cues[2].ball == 2);
}
