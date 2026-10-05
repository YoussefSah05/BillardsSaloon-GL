#include "sim/motion.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace BilliardsSaloon::Sim
{
    namespace
    {
        constexpr double INF = std::numeric_limits<double>::infinity();
        const glm::dvec3 UNIT_Z {0.0, 0.0, 1.0};

        glm::dvec3 horizontal(const glm::dvec3& vector)
        {
            return {vector.x, vector.y, 0.0};
        }

        glm::dvec3 unit(const glm::dvec3& vector)
        {
            const double length = glm::length(vector);
            return (length > 0.0) ? vector / length : glm::dvec3(0.0);
        }

        // Vertical spin decays at 5·μ_sp·g / 2R, never past zero.
        double decaySpin(double wz, const BallParams& params, double t)
        {
            if ((t <= 0.0) || (std::abs(wz) < EPS))
            {
                return wz;
            }
            const double alpha = 5.0 * params.u_sp() * params.g / (2.0 * params.R);
            const double active = std::min(t, std::abs(wz) / alpha);
            return wz - std::copysign(alpha * active, wz);
        }

        BallState evolveSliding(const BallState& state, const BallParams& params, double t)
        {
            const glm::dvec3 u0 = unit(relativeVelocity(state, params.R));
            const double a = params.u_s * params.g;

            BallState next = state;
            next.r = state.r + state.v * t - 0.5 * a * t * t * u0;
            next.v = state.v - a * t * u0;
            next.w = state.w + (5.0 * a / (2.0 * params.R)) * t * glm::cross(UNIT_Z, u0);
            next.w.z = decaySpin(state.w.z, params, t);
            return next;
        }

        BallState evolveRolling(const BallState& state, const BallParams& params, double t)
        {
            const glm::dvec3 direction = unit(horizontal(state.v));
            const double a = params.u_r * params.g;

            BallState next = state;
            next.r = state.r + state.v * t - 0.5 * a * t * t * direction;
            next.v = state.v - a * t * direction;
            // Rolling without slipping: ω_xy = ẑ × v / R.
            const glm::dvec3 rolling = glm::cross(UNIT_Z, next.v) / params.R;
            next.w = {rolling.x, rolling.y, decaySpin(state.w.z, params, t)};
            return next;
        }
    }

    glm::dvec3 relativeVelocity(const BallState& state, double R)
    {
        return state.v + glm::cross(state.w, glm::dvec3(0.0, 0.0, -R));
    }

    double slideTime(const BallState& state, const BallParams& params)
    {
        return (params.u_s == 0.0)
            ? INF
            : 2.0 * glm::length(relativeVelocity(state, params.R)) / (7.0 * params.u_s * params.g);
    }

    double rollTime(const BallState& state, const BallParams& params)
    {
        return (params.u_r == 0.0) ? INF : glm::length(state.v) / (params.u_r * params.g);
    }

    double spinTime(const BallState& state, const BallParams& params)
    {
        return (params.u_sp() == 0.0)
            ? INF
            : std::abs(state.w.z) * 2.0 / 5.0 * params.R / params.u_sp() / params.g;
    }

    double transitionTime(const BallState& state, const BallParams& params)
    {
        switch (state.s)
        {
            case MotionState::Sliding:
                return slideTime(state, params);
            case MotionState::Rolling:
                return rollTime(state, params);
            case MotionState::Spinning:
                return spinTime(state, params);
            case MotionState::Stationary:
            case MotionState::Pocketed:
                return INF;
        }
        return INF;
    }

    MotionState classify(const BallState& state, double R)
    {
        if (state.r.z < 0.0)
        {
            return MotionState::Pocketed;
        }
        if (glm::length(relativeVelocity(state, R)) > EPS)
        {
            return MotionState::Sliding;
        }
        if (glm::length(horizontal(state.v)) > EPS)
        {
            return MotionState::Rolling;
        }
        if (std::abs(state.w.z) > EPS)
        {
            return MotionState::Spinning;
        }
        return MotionState::Stationary;
    }

    BallState evolve(const BallState& state, const BallParams& params, double t)
    {
        BallState current = state;
        double remaining = t;

        // At most three transitions: sliding → rolling → spinning → stationary.
        for (int step = 0; step < 4 && remaining > 0.0; ++step)
        {
            switch (current.s)
            {
                case MotionState::Stationary:
                case MotionState::Pocketed:
                    return current;

                case MotionState::Sliding:
                {
                    const double until = slideTime(current, params);
                    if (remaining < until)
                    {
                        return evolveSliding(current, params, remaining);
                    }
                    current = evolveSliding(current, params, until);
                    // Pin the rolling constraint exactly to stop round-off drift.
                    const glm::dvec3 rolling = glm::cross(UNIT_Z, current.v) / params.R;
                    current.w = {rolling.x, rolling.y, current.w.z};
                    current.v.z = 0.0;
                    current.s = (glm::length(current.v) > EPS) ? MotionState::Rolling
                              : (std::abs(current.w.z) > EPS) ? MotionState::Spinning
                              : MotionState::Stationary;
                    remaining -= until;
                    break;
                }

                case MotionState::Rolling:
                {
                    const double until = rollTime(current, params);
                    if (remaining < until)
                    {
                        return evolveRolling(current, params, remaining);
                    }
                    current = evolveRolling(current, params, until);
                    current.v = glm::dvec3(0.0);
                    current.w = {0.0, 0.0, current.w.z};
                    current.s = (std::abs(current.w.z) > EPS) ? MotionState::Spinning : MotionState::Stationary;
                    remaining -= until;
                    break;
                }

                case MotionState::Spinning:
                {
                    const double until = spinTime(current, params);
                    if (remaining < until)
                    {
                        current.w.z = decaySpin(current.w.z, params, remaining);
                        return current;
                    }
                    current.w = glm::dvec3(0.0);
                    current.s = MotionState::Stationary;
                    remaining -= until;
                    break;
                }
            }
        }

        return current;
    }

    PositionPolynomial positionPolynomial(const BallState& state, const BallParams& params)
    {
        PositionPolynomial p;
        p.c0 = state.r;

        switch (state.s)
        {
            case MotionState::Sliding:
                p.c1 = state.v;
                p.c2 = -0.5 * params.u_s * params.g * unit(relativeVelocity(state, params.R));
                break;
            case MotionState::Rolling:
                p.c1 = state.v;
                p.c2 = -0.5 * params.u_r * params.g * unit(horizontal(state.v));
                break;
            case MotionState::Stationary:
            case MotionState::Spinning:
            case MotionState::Pocketed:
                break;
        }

        return p;
    }

    double kineticEnergy(const BallState& state, const BallParams& params)
    {
        const double inertia = 0.4 * params.m * params.R * params.R;
        return 0.5 * params.m * glm::dot(state.v, state.v) + 0.5 * inertia * glm::dot(state.w, state.w);
    }
}
