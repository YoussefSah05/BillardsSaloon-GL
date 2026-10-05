#include "sim/simulate.h"

#include "sim/events.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace BilliardsSaloon::Sim
{
    namespace
    {
        constexpr double INF = std::numeric_limits<double>::infinity();

        glm::dvec2 xy(const glm::dvec3& v)
        {
            return {v.x, v.y};
        }

        // Move the ball along the normal so it just touches the straight
        // cushion's nose, plus MIN_DIST.
        void kissLinear(BallState& ball, const LinearCushion& cushion, const BallParams& params)
        {
            const glm::dvec2 axis = glm::normalize(xy(cushion.p2 - cushion.p1));
            glm::dvec2 normal(-axis.y, axis.x);
            const double signedDistance = glm::dot(xy(ball.r - cushion.p1), normal);
            if (signedDistance < 0.0)
            {
                normal = -normal;
            }
            const double reach = cushion.noseRadius + params.R;
            const double heightGap = cushion.p1.z - params.R;
            const double contact = std::sqrt(reach * reach - heightGap * heightGap) + MIN_DIST;
            const double correction = contact - std::abs(signedDistance);
            ball.r += glm::dvec3(normal.x, normal.y, 0.0) * correction;
        }

        void kissCircular(BallState& ball, const CircularCushion& cushion, const BallParams& params)
        {
            const glm::dvec2 away = glm::normalize(xy(ball.r - cushion.center));
            const glm::dvec2 at = xy(cushion.center) + away * (cushion.radius + params.R + MIN_DIST);
            ball.r.x = at.x;
            ball.r.y = at.y;
        }
    }

    double ShotTrajectory::duration() const
    {
        return events.empty() ? 0.0 : events.back().time;
    }

    std::vector<BallState> ShotTrajectory::stateAt(double t) const
    {
        if (events.empty())
        {
            return {};
        }

        // The last event at or before t.
        std::size_t index = 0;
        while ((index + 1 < events.size()) && (events[index + 1].time <= t))
        {
            ++index;
        }

        std::vector<BallState> result = states[index];
        const double dt = t - events[index].time;
        if (dt > 0.0)
        {
            for (BallState& ball : result)
            {
                ball = evolve(ball, params, dt);
            }
        }
        return result;
    }

    ShotTrajectory simulateShot(
        const Table& table,
        std::vector<BallState> balls,
        int cueBall,
        const CueStrike& strike,
        const BallParams& params,
        const CueSpecs& cue,
        const SimulationLimits& limits)
    {
        ShotTrajectory trajectory;
        trajectory.params = params;

        const std::size_t cueIndex = static_cast<std::size_t>(cueBall);
        balls[cueIndex] = resolveStrike(balls[cueIndex], strike, params, cue);
        trajectory.events.push_back(ShotEvent{EventType::Strike, 0.0, cueBall});
        trajectory.states.push_back(balls);

        double now = 0.0;
        const std::size_t count = balls.size();

        for (int eventCount = 0; eventCount < limits.maxEvents; ++eventCount)
        {
            // Each ball's polynomial holds until its next transition.
            std::vector<double> horizons(count);
            ShotEvent next;
            double best = INF;

            for (std::size_t i = 0; i < count; ++i)
            {
                horizons[i] = transitionTime(balls[i], params);
                if (horizons[i] < best)
                {
                    best = horizons[i];
                    next = ShotEvent{EventType::Transition, 0.0, static_cast<int>(i)};
                }
            }

            for (std::size_t i = 0; i < count; ++i)
            {
                if (balls[i].s == MotionState::Pocketed)
                {
                    continue;
                }

                for (std::size_t j = i + 1; j < count; ++j)
                {
                    const double horizon = std::min({horizons[i], horizons[j], best});
                    const double t = ballBallCollisionTime(balls[i], balls[j], params, horizon);
                    if (t < best)
                    {
                        best = t;
                        next = ShotEvent{EventType::BallBall, 0.0, static_cast<int>(i), static_cast<int>(j)};
                    }
                }

                const double horizon = std::min(horizons[i], best);
                for (std::size_t k = 0; k < table.linear.size(); ++k)
                {
                    const double t = linearCushionCollisionTime(balls[i], table.linear[k], params, std::min(horizon, best));
                    if (t < best)
                    {
                        best = t;
                        next = ShotEvent{EventType::LinearCushion, 0.0, static_cast<int>(i), static_cast<int>(k)};
                    }
                }
                for (std::size_t k = 0; k < table.circular.size(); ++k)
                {
                    const double t = circularCushionCollisionTime(balls[i], table.circular[k], params, std::min(horizon, best));
                    if (t < best)
                    {
                        best = t;
                        next = ShotEvent{EventType::CircularCushion, 0.0, static_cast<int>(i), static_cast<int>(k)};
                    }
                }
                for (std::size_t k = 0; k < table.pockets.size(); ++k)
                {
                    const double t = pocketEntryTime(balls[i], table.pockets[k], params, std::min(horizon, best));
                    if (t < best)
                    {
                        best = t;
                        next = ShotEvent{EventType::Pocket, 0.0, static_cast<int>(i), static_cast<int>(k)};
                    }
                }
            }

            if (!std::isfinite(best))
            {
                return trajectory;   // everything is at rest or pocketed
            }

            // Advance every ball to the event.
            for (BallState& ball : balls)
            {
                ball = evolve(ball, params, best);
            }
            now += best;
            next.time = now;

            BallState& ball = balls[static_cast<std::size_t>(next.ball)];
            switch (next.type)
            {
                case EventType::Transition:
                    // evolve() already crossed the boundary at exactly this time.
                    next.from = trajectory.states.back()[static_cast<std::size_t>(next.ball)].s;
                    next.to = ball.s;
                    break;

                case EventType::BallBall:
                    resolveBallBall(ball, balls[static_cast<std::size_t>(next.other)], params);
                    break;

                case EventType::LinearCushion:
                {
                    const LinearCushion& cushion = table.linear[static_cast<std::size_t>(next.other)];
                    const glm::dvec3 normal = linearCushionNormal(ball, cushion);
                    kissLinear(ball, cushion, params);
                    ball = resolveCushion(ball, normal, cushion.p1.z, params);
                    break;
                }

                case EventType::CircularCushion:
                {
                    const CircularCushion& cushion = table.circular[static_cast<std::size_t>(next.other)];
                    const glm::dvec3 normal = circularCushionNormal(ball, cushion);
                    kissCircular(ball, cushion, params);
                    ball = resolveCushion(ball, normal, cushion.center.z, params);
                    break;
                }

                case EventType::Pocket:
                    ball = resolvePocket(ball);
                    break;

                case EventType::Strike:
                    break;
            }

            trajectory.events.push_back(next);
            trajectory.states.push_back(balls);
        }

        trajectory.complete = false;
        return trajectory;
    }
}
