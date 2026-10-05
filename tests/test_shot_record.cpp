#include "rules/shot_record.h"

#include <doctest/doctest.h>

using namespace BilliardsSaloon;
using namespace BilliardsSaloon::Sim;

namespace
{
    ShotEvent event(EventType type, double time, int ball, int other = -1)
    {
        ShotEvent e;
        e.type = type;
        e.time = time;
        e.ball = ball;
        e.other = other;
        return e;
    }
}

TEST_CASE("shot record: first contact, cushions after contact and pockets with their pocket")
{
    // Simulator indices 0..3 hold the cue ball and balls 5, 2 and 9.
    const std::vector<int> numbers {0, 5, 2, 9};

    ShotTrajectory trajectory;
    trajectory.events = {
        event(EventType::Strike, 0.0, 0),
        event(EventType::LinearCushion, 0.1, 0, 3),     // cue ball to a cushion before contact
        event(EventType::BallBall, 0.3, 2, 0),          // cue ball meets the 2
        event(EventType::BallBall, 0.4, 2, 1),          // the 2 meets the 5
        event(EventType::CircularCushion, 0.6, 1, 2),   // the 5 hits a jaw
        event(EventType::Pocket, 0.8, 1, 4),            // the 5 drops in pocket 4
        event(EventType::LinearCushion, 0.9, 2, 7),
        event(EventType::LinearCushion, 1.0, 2, 8),     // same ball twice counts once
        event(EventType::Pocket, 1.5, 0, 1),            // scratch in pocket 1
    };

    const Rules::ShotRecord record = Rules::recordShot(trajectory, numbers);
    CHECK(record.firstContact == 2);
    CHECK(record.railAfterContact);
    CHECK(record.objectBallsToRail == 2);
    REQUIRE(record.pots.size() == 2);
    CHECK(record.pots[0].ball == 5);
    CHECK(record.pots[0].pocket == 4);
    CHECK(record.potted(Rules::CUE_BALL));
    CHECK(record.pocketOf(Rules::CUE_BALL) == 1);
    CHECK(record.objectPots() == std::vector<int>{5});
}

TEST_CASE("shot record: a cushion before contact is not a cushion after contact")
{
    const std::vector<int> numbers {0, 1};
    ShotTrajectory trajectory;
    trajectory.events = {
        event(EventType::LinearCushion, 0.1, 0, 3),
        event(EventType::BallBall, 0.3, 0, 1),
    };

    const Rules::ShotRecord record = Rules::recordShot(trajectory, numbers);
    CHECK(record.firstContact == 1);
    CHECK_FALSE(record.railAfterContact);
    CHECK(record.objectBallsToRail == 0);
}
