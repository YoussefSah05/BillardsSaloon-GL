#include "sim/events.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace BilliardsSaloon::Sim;

namespace
{
    BallState rolling(double x, double y, double vx, double vy, const BallParams& params)
    {
        BallState state;
        state.r = {x, y, params.R};
        state.v = {vx, vy, 0.0};
        const glm::dvec3 w = glm::cross(glm::dvec3(0, 0, 1), state.v) / params.R;
        state.w = w;
        state.s = classify(state, params.R);
        return state;
    }

    BallState resting(double x, double y, const BallParams& params)
    {
        BallState state;
        state.r = {x, y, params.R};
        state.s = MotionState::Stationary;
        return state;
    }

    // Time for a rolling ball to cover distance d from speed v.
    double rollingTimeFor(double d, double v, const BallParams& params)
    {
        const double a = params.u_r * params.g;
        return (v - std::sqrt(v * v - 2.0 * a * d)) / a;
    }
}

TEST_CASE("the pocket table has the expected parts and is symmetric")
{
    const Table table = buildPocketTable(PocketTableSpec{});
    CHECK(table.linear.size() == 18);
    CHECK(table.circular.size() == 12);
    CHECK(table.pockets.size() == 6);

    // Main rails lie on the playing-surface boundary.
    CHECK(table.linear[0].p1.x == doctest::Approx(0.0));
    CHECK(table.linear[2].p1.x == doctest::Approx(table.width));
    CHECK(table.linear[4].p1.y == doctest::Approx(0.0));
    CHECK(table.linear[5].p1.y == doctest::Approx(table.length));

    // Side pockets centred on the long rails.
    CHECK(table.pockets[1].center.y == doctest::Approx(table.length / 2.0));
    CHECK(table.pockets[4].center.x == doctest::Approx(table.width + PocketTableSpec{}.sidePocketDepth));
}

TEST_CASE("a rolling ball hits a resting ball when the centres are 2R apart")
{
    const BallParams params;
    const BallState cue = rolling(0.5, 0.5, 1.0, 0.0, params);
    const BallState object = resting(0.8, 0.5, params);

    const double t = ballBallCollisionTime(cue, object, params, transitionTime(cue, params));
    CHECK(t == doctest::Approx(rollingTimeFor(0.3 - 2.0 * params.R, 1.0, params)).epsilon(1e-10));
}

TEST_CASE("balls moving apart or missing never collide")
{
    const BallParams params;
    const BallState away = rolling(0.5, 0.5, -1.0, 0.0, params);
    const BallState object = resting(0.8, 0.5, params);
    CHECK(std::isinf(ballBallCollisionTime(away, object, params, 10.0)));

    const BallState wide = rolling(0.5, 0.5, 1.0, 0.0, params);
    const BallState offLine = resting(0.8, 0.5 + 2.1 * params.R, params);
    CHECK(std::isinf(ballBallCollisionTime(wide, offLine, params, 10.0)));
}

TEST_CASE("a ball reaches a straight cushion at the nose contact distance")
{
    const BallParams params;
    const Table table = buildPocketTable(PocketTableSpec{});
    const LinearCushion& bottom = table.linear[4];   // y = 0 rail

    const BallState ball = rolling(0.6, 0.4, 0.0, -1.5, params);
    const double reach = bottom.noseRadius + params.R;
    const double gap = bottom.p1.z - params.R;
    const double contact = std::sqrt(reach * reach - gap * gap);

    const double t = linearCushionCollisionTime(ball, bottom, params, transitionTime(ball, params));
    CHECK(t == doctest::Approx(rollingTimeFor(0.4 - contact, 1.5, params)).epsilon(1e-10));

    const glm::dvec3 normal = linearCushionNormal(ball, bottom);
    CHECK(normal.y == doctest::Approx(-1.0));
}

TEST_CASE("a ball heading into a corner pocket enters it")
{
    const BallParams params;
    const Table table = buildPocketTable(PocketTableSpec{});
    const Pocket& corner = table.pockets[0];   // at the origin corner

    const BallState ball = rolling(0.3, 0.3, -1.0, -1.0, params);
    const double t = pocketEntryTime(ball, corner, params, transitionTime(ball, params));
    REQUIRE(std::isfinite(t));

    // At that moment the centre is exactly one pocket radius from the pocket centre.
    const BallState at = evolve(ball, params, t);
    CHECK(glm::length(glm::dvec2(at.r.x, at.r.y) - glm::dvec2(corner.center.x, corner.center.y)) ==
          doctest::Approx(corner.radius).epsilon(1e-9));
}

TEST_CASE("the horizon cuts off events beyond the next transition")
{
    const BallParams params;
    const BallState cue = rolling(0.5, 0.5, 1.0, 0.0, params);
    const BallState object = resting(0.8, 0.5, params);
    CHECK(std::isinf(ballBallCollisionTime(cue, object, params, 0.01)));
}
