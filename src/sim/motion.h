#pragma once

#include <glm/glm.hpp>

#include <cstdint>

// Event-based billiards simulation, ported from pooltool (Kiefl, JOSS 2024,
// Apache-2.0) and Leckie & Greenspan. The simulator uses pooltool's frame:
// z is up, the cloth is z = 0, ball centres rest at z = R. See
// docs/design/PHYSICS.md for the equations.
namespace BilliardsSaloon::Sim
{
    enum class MotionState : std::uint8_t
    {
        Stationary,
        Spinning,   // turning about the vertical only
        Sliding,    // contact point slips on the cloth
        Rolling,
        Pocketed
    };

    struct BallParams
    {
        double m {0.170097};          // kg
        double R {0.028575};          // m
        double u_s {0.2};             // sliding friction
        double u_r {0.01};            // rolling resistance
        double u_sp_proportionality {10.0 * 2.0 / 5.0 / 9.0};
        double e_b {0.95};            // ball-ball restitution
        double e_c {0.85};            // ball-cushion restitution
        double f_c {0.2};             // ball-cushion friction
        double g {9.81};

        // Spinning friction coefficient; scales with the ball radius.
        [[nodiscard]] double u_sp() const { return u_sp_proportionality * R; }
    };

    struct BallState
    {
        glm::dvec3 r {0.0};   // centre position
        glm::dvec3 v {0.0};   // linear velocity
        glm::dvec3 w {0.0};   // angular velocity
        MotionState s {MotionState::Stationary};
    };

    // Speeds at or below this count as zero.
    inline constexpr double EPS = 2.220446049250313e-14;   // 100 × machine epsilon

    // Velocity of the contact point with the cloth: v + ω × (−R ẑ).
    [[nodiscard]] glm::dvec3 relativeVelocity(const BallState& state, double R);

    [[nodiscard]] double slideTime(const BallState& state, const BallParams& params);
    [[nodiscard]] double rollTime(const BallState& state, const BallParams& params);
    [[nodiscard]] double spinTime(const BallState& state, const BallParams& params);

    // Time until the ball's motion state changes without a collision
    // (+infinity for stationary or pocketed balls).
    [[nodiscard]] double transitionTime(const BallState& state, const BallParams& params);

    // Motion state implied by the kinematics, e.g. right after an impact.
    [[nodiscard]] MotionState classify(const BallState& state, double R);

    // Closed-form evolution by t seconds, passing through any transitions.
    [[nodiscard]] BallState evolve(const BallState& state, const BallParams& params, double t);

    // r(t) = c0 + c1·t + c2·t², valid until the ball's next transition.
    struct PositionPolynomial
    {
        glm::dvec3 c0 {0.0};
        glm::dvec3 c1 {0.0};
        glm::dvec3 c2 {0.0};
    };

    [[nodiscard]] PositionPolynomial positionPolynomial(const BallState& state, const BallParams& params);

    // Linear plus rotational kinetic energy.
    [[nodiscard]] double kineticEnergy(const BallState& state, const BallParams& params);
}
