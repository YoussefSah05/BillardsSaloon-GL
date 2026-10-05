#include "sim/motion.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace BilliardsSaloon::Sim;

namespace
{
    BallState stunShot(double speed)
    {
        BallState state;
        state.r = {0.5, 0.5, 0.028575};
        state.v = {speed, 0.0, 0.0};
        state.s = MotionState::Sliding;
        return state;
    }

    BallState rollingBall(double speed, const BallParams& params)
    {
        BallState state;
        state.r = {0.5, 0.5, params.R};
        state.v = {0.0, speed, 0.0};
        state.w = {-speed / params.R, 0.0, 0.0};   // ẑ × v / R
        state.s = classify(state, params.R);
        return state;
    }
}

TEST_CASE("a stun shot rolls at 5/7 of its speed after the slide time")
{
    const BallParams params;
    const BallState start = stunShot(2.0);
    const double tSlide = slideTime(start, params);
    CHECK(tSlide == doctest::Approx(2.0 * 2.0 / (7.0 * params.u_s * params.g)));

    const BallState rolling = evolve(start, params, tSlide);
    CHECK(rolling.s == MotionState::Rolling);
    CHECK(glm::length(rolling.v) == doctest::Approx(2.0 * 5.0 / 7.0).epsilon(1e-12));
    CHECK(glm::length(relativeVelocity(rolling, params.R)) == doctest::Approx(0.0).epsilon(1e-12));
}

TEST_CASE("a rolling ball stops after v² / (2 μ_r g)")
{
    const BallParams params;
    const BallState start = rollingBall(1.2, params);
    CHECK(start.s == MotionState::Rolling);

    const BallState stopped = evolve(start, params, 100.0);
    CHECK(stopped.s == MotionState::Stationary);
    const double distance = glm::length(stopped.r - start.r);
    CHECK(distance == doctest::Approx(1.2 * 1.2 / (2.0 * params.u_r * params.g)).epsilon(1e-12));
}

TEST_CASE("pure vertical spin decays at 5 μ_sp g / 2R")
{
    const BallParams params;
    BallState spinning;
    spinning.r = {0.5, 0.5, params.R};
    spinning.w = {0.0, 0.0, 30.0};
    spinning.s = classify(spinning, params.R);
    CHECK(spinning.s == MotionState::Spinning);

    const double alpha = 5.0 * params.u_sp() * params.g / (2.0 * params.R);
    CHECK(spinTime(spinning, params) == doctest::Approx(30.0 / alpha));
    CHECK(evolve(spinning, params, 0.5).w.z == doctest::Approx(30.0 - alpha * 0.5));
    CHECK(evolve(spinning, params, 60.0).s == MotionState::Stationary);
}

TEST_CASE("evolving in two steps equals evolving once")
{
    const BallParams params;
    BallState start = stunShot(3.0);
    start.w = {12.0, -40.0, 25.0};   // backspin, side spin
    start.s = classify(start, params.R);

    for (const double split : {0.05, 0.3, 1.1, 4.0})
    {
        const BallState once = evolve(start, params, 6.0);
        const BallState twice = evolve(evolve(start, params, split), params, 6.0 - split);
        CHECK(glm::length(once.r - twice.r) < 1e-9);
        CHECK(glm::length(once.v - twice.v) < 1e-9);
        CHECK(glm::length(once.w - twice.w) < 1e-7);
        CHECK(once.s == twice.s);
    }
}

TEST_CASE("the position polynomial matches evolution within a motion state")
{
    const BallParams params;
    BallState start = stunShot(2.5);
    start.w = {5.0, 30.0, -10.0};
    start.s = classify(start, params.R);

    const PositionPolynomial p = positionPolynomial(start, params);
    const double until = transitionTime(start, params);
    for (const double fraction : {0.1, 0.5, 0.99})
    {
        const double t = fraction * until;
        const glm::dvec3 predicted = p.c0 + p.c1 * t + p.c2 * t * t;
        CHECK(glm::length(predicted - evolve(start, params, t).r) < 1e-12);
    }
}

TEST_CASE("kinetic energy never increases while a ball evolves")
{
    const BallParams params;
    BallState start = stunShot(4.0);
    start.w = {-80.0, 20.0, 50.0};
    start.s = classify(start, params.R);

    double previous = kineticEnergy(start, params);
    for (int i = 1; i <= 400; ++i)
    {
        const double current = kineticEnergy(evolve(start, params, 0.02 * i), params);
        CHECK(current <= previous + 1e-12);
        previous = current;
    }
}
