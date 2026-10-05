#include "sim/simulate.h"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using namespace BilliardsSaloon::Sim;

namespace
{
    BallState at(double x, double y, const BallParams& params)
    {
        BallState state;
        state.r = {x, y, params.R};
        return state;
    }

    double totalEnergy(const std::vector<BallState>& balls, const BallParams& params)
    {
        double energy = 0.0;
        for (const BallState& ball : balls)
        {
            if (ball.s != MotionState::Pocketed)
            {
                energy += kineticEnergy(ball, params);
            }
        }
        return energy;
    }

    // Standard 15-ball triangle with its apex on the foot spot, cue ball on the head string.
    std::vector<BallState> rackedTable(const Table& table, const BallParams& params)
    {
        std::vector<BallState> balls {at(table.width / 2.0, table.length / 4.0, params)};
        const double spacing = 2.0 * params.R * 1.0001;
        const double rowStep = std::sqrt(3.0) / 2.0 * spacing;
        for (int row = 0; row < 5; ++row)
        {
            for (int column = 0; column <= row; ++column)
            {
                const double x = table.width / 2.0 + (column - row / 2.0) * spacing;
                const double y = 0.75 * table.length + row * rowStep;
                balls.push_back(at(x, y, params));
            }
        }
        return balls;
    }
}

TEST_CASE("a centre-ball strike is a pure stun shot along the cue line")
{
    const BallParams params;
    const CueSpecs cue;
    const BallState ball = resolveStrike(at(0.5, 0.5, params), CueStrike{2.0, 90.0}, params, cue);

    const double expected = 2.0 * 2.0 / (1.0 + params.m / cue.M);
    CHECK(ball.v.y == doctest::Approx(expected));
    CHECK(ball.v.x == doctest::Approx(0.0).epsilon(1e-12));
    CHECK(glm::length(ball.w) == doctest::Approx(0.0).epsilon(1e-9));
    CHECK(ball.s == MotionState::Sliding);
}

TEST_CASE("a high hit gives topspin and a low hit backspin")
{
    const BallParams params;
    const BallState follow = resolveStrike(at(0.5, 0.5, params), CueStrike{2.0, 90.0, 0.0, 0.0, 0.4}, params);
    const BallState draw = resolveStrike(at(0.5, 0.5, params), CueStrike{2.0, 90.0, 0.0, 0.0, -0.4}, params);

    // Natural roll for motion along +y is ω_x = -v_y / R.
    CHECK(follow.w.x < 0.0);
    CHECK(draw.w.x > 0.0);
}

TEST_CASE("side spin squirts the cue ball away from the english")
{
    const BallParams params;
    const BallState left = resolveStrike(at(0.5, 0.5, params), CueStrike{2.0, 90.0, 0.0, 0.5, 0.0}, params);
    CHECK(left.w.z != doctest::Approx(0.0));
    // Left english (a > 0) pushes the ball to the right of the +y cue line.
    CHECK(left.v.x > 0.0);
}

TEST_CASE("a full stun hit passes nearly all speed to the object ball")
{
    const BallParams params;
    BallState cue = at(0.5, 0.5, params);
    cue.v = {1.0, 0.0, 0.0};
    cue.s = classify(cue, params.R);
    BallState object = at(0.5 + 2.0 * params.R, 0.5, params);

    resolveBallBall(cue, object, params);
    CHECK(object.v.x == doctest::Approx(0.5 * (1.0 + params.e_b)));
    CHECK(cue.v.x == doctest::Approx(0.5 * (1.0 - params.e_b)));
}

TEST_CASE("a cushion reverses the approach and loses energy")
{
    const BallParams params;
    BallState ball = at(0.5, 0.3, params);
    ball.v = {0.0, -2.0, 0.0};
    ball.w = {2.0 / params.R, 0.0, 0.0};   // rolling towards -y
    ball.s = classify(ball, params.R);

    const BallState after = resolveCushion(ball, glm::dvec3(0.0, -1.0, 0.0), 0.64 * 2.0 * params.R, params);
    CHECK(after.v.y > 0.0);
    CHECK(after.v.y < 2.0);
    CHECK(kineticEnergy(after, params) < kineticEnergy(ball, params));
}

TEST_CASE("a lone ball banks off a cushion and comes to rest on the table")
{
    const BallParams params;
    const Table table = buildPocketTable(PocketTableSpec{});
    const ShotTrajectory shot =
        simulateShot(table, {at(0.635, 0.6, params)}, 0, CueStrike{2.5, 270.0}, params);

    REQUIRE(shot.complete);
    bool hitCushion = false;
    for (const ShotEvent& event : shot.events)
    {
        hitCushion = hitCushion || (event.type == EventType::LinearCushion);
    }
    CHECK(hitCushion);

    const BallState& end = shot.finalState()[0];
    CHECK(end.s == MotionState::Stationary);
    CHECK(end.r.x == doctest::Approx(0.635).epsilon(1e-6));   // straight up and down
    CHECK(end.r.y > params.R);
    CHECK(end.r.y < table.length - params.R);
}

TEST_CASE("a ball sent down the diagonal drops in the corner pocket")
{
    const BallParams params;
    const Table table = buildPocketTable(PocketTableSpec{});
    const ShotTrajectory shot =
        simulateShot(table, {at(0.4, 0.4, params)}, 0, CueStrike{2.0, 225.0}, params);

    REQUIRE(shot.complete);
    CHECK(shot.finalState()[0].s == MotionState::Pocketed);
    CHECK(shot.events.back().type == EventType::Pocket);
}

TEST_CASE("a hard break settles with no overlaps and no energy gained between events")
{
    const BallParams params;
    const Table table = buildPocketTable(PocketTableSpec{});
    const std::vector<BallState> rack = rackedTable(table, params);
    const ShotTrajectory shot = simulateShot(table, rack, 0, CueStrike{9.0, 90.0}, params);

    REQUIRE(shot.complete);
    CHECK(shot.events.size() > 30);

    // Energy only ever drops after the strike.
    for (std::size_t i = 2; i < shot.states.size(); ++i)
    {
        CHECK(totalEnergy(shot.states[i], params) <= totalEnergy(shot.states[i - 1], params) * (1.0 + 1e-9) + 1e-12);
    }

    const std::vector<BallState>& end = shot.finalState();
    for (std::size_t i = 0; i < end.size(); ++i)
    {
        CHECK(((end[i].s == MotionState::Stationary) || (end[i].s == MotionState::Pocketed)));
        if (end[i].s == MotionState::Pocketed)
        {
            continue;
        }
        for (std::size_t j = i + 1; j < end.size(); ++j)
        {
            if (end[j].s != MotionState::Pocketed)
            {
                CHECK(glm::length(end[i].r - end[j].r) >= 2.0 * params.R - 1e-6);
            }
        }
        CHECK(end[i].r.x > -0.02);
        CHECK(end[i].r.x < table.width + 0.02);
        CHECK(end[i].r.y > -0.02);
        CHECK(end[i].r.y < table.length + 0.02);
    }
}

TEST_CASE("the same shot always produces the same trajectory")
{
    const BallParams params;
    const Table table = buildPocketTable(PocketTableSpec{});
    const std::vector<BallState> rack = rackedTable(table, params);
    const ShotTrajectory first = simulateShot(table, rack, 0, CueStrike{7.5, 91.0, 0.0, 0.2, -0.3}, params);
    const ShotTrajectory second = simulateShot(table, rack, 0, CueStrike{7.5, 91.0, 0.0, 0.2, -0.3}, params);

    REQUIRE(first.events.size() == second.events.size());
    for (std::size_t i = 0; i < first.finalState().size(); ++i)
    {
        CHECK(first.finalState()[i].r == second.finalState()[i].r);
    }
}

TEST_CASE("stateAt reproduces the recorded state at each event")
{
    const BallParams params;
    const Table table = buildPocketTable(PocketTableSpec{});
    const ShotTrajectory shot =
        simulateShot(table, {at(0.635, 0.6, params)}, 0, CueStrike{2.5, 270.0}, params);

    for (std::size_t i = 1; i < shot.events.size(); ++i)
    {
        const double t = shot.events[i].time;
        const BallState just = shot.stateAt(t - 1e-9)[0];
        CHECK(glm::length(just.r - shot.states[i][0].r) < 1e-6);
    }
}
