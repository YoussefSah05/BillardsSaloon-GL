#include "sim/events.h"

#include "sim/roots.h"

#include <cmath>
#include <limits>

namespace BilliardsSaloon::Sim
{
    namespace
    {
        constexpr double INF = std::numeric_limits<double>::infinity();

        bool isMoving(const BallState& state)
        {
            return (state.s == MotionState::Sliding) || (state.s == MotionState::Rolling);
        }

        // (|c0 + c1 t + c2 t²|² − distance²) / 2 as a quartic in t, over the
        // horizontal components only (all balls stay on the cloth).
        Polynomial horizontalDistanceQuartic(const glm::dvec2& c0, const glm::dvec2& c1, const glm::dvec2& c2, double distance)
        {
            Polynomial p;
            p.c = {
                (glm::dot(c0, c0) - distance * distance) / 2.0,
                glm::dot(c1, c0),
                glm::dot(c2, c0) + glm::dot(c1, c1) / 2.0,
                glm::dot(c2, c1),
                glm::dot(c2, c2) / 2.0
            };
            return p;
        }

        glm::dvec2 xy(const glm::dvec3& v)
        {
            return {v.x, v.y};
        }
    }

    double ballBallCollisionTime(const BallState& a, const BallState& b, const BallParams& params, double horizon)
    {
        if ((a.s == MotionState::Pocketed) || (b.s == MotionState::Pocketed) || (!isMoving(a) && !isMoving(b)))
        {
            return INF;
        }

        const PositionPolynomial pa = positionPolynomial(a, params);
        const PositionPolynomial pb = positionPolynomial(b, params);
        const Polynomial separation = horizontalDistanceQuartic(
            xy(pa.c0 - pb.c0), xy(pa.c1 - pb.c1), xy(pa.c2 - pb.c2), 2.0 * params.R);

        return firstClosingRoot(separation, 0.0, horizon);
    }

    double linearCushionCollisionTime(const BallState& ball, const LinearCushion& cushion, const BallParams& params, double horizon)
    {
        if (!isMoving(ball))
        {
            return INF;
        }

        const glm::dvec2 axis = glm::normalize(xy(cushion.p2 - cushion.p1));
        const glm::dvec2 normal(-axis.y, axis.x);
        const double length = glm::length(xy(cushion.p2 - cushion.p1));

        // Horizontal distance at which the ball (centre at z = R) touches the
        // nose cylinder (axis at the cushion height).
        const double reach = cushion.noseRadius + params.R;
        const double heightGap = cushion.p1.z - params.R;
        if (reach <= std::abs(heightGap))
        {
            return INF;   // the nose passes over or under the ball
        }
        const double contactDistance = std::sqrt(reach * reach - heightGap * heightGap);

        const PositionPolynomial p = positionPolynomial(ball, params);
        const glm::dvec2 offset = xy(p.c0 - cushion.p1);
        const double side = (glm::dot(offset, normal) >= 0.0) ? 1.0 : -1.0;

        // gap(t) = side · n·(r(t) − p1) − contactDistance, a quadratic.
        Polynomial gap;
        gap.c = {
            side * glm::dot(offset, normal) - contactDistance,
            side * glm::dot(xy(p.c1), normal),
            side * glm::dot(xy(p.c2), normal),
            0.0,
            0.0
        };

        const Polynomial dgap = gap.derivative();
        for (const double t : realRootsInInterval(gap, 0.0, horizon))
        {
            if ((t <= 0.0) || (dgap(t) >= 0.0))
            {
                continue;   // not closing
            }
            const glm::dvec2 at = xy(p.c0 + p.c1 * t + p.c2 * t * t);
            const double along = glm::dot(at - xy(cushion.p1), axis);
            if ((along > 0.0) && (along < length))
            {
                return t;
            }
        }
        return INF;
    }

    double circularCushionCollisionTime(const BallState& ball, const CircularCushion& cushion, const BallParams& params, double horizon)
    {
        if (!isMoving(ball))
        {
            return INF;
        }

        const PositionPolynomial p = positionPolynomial(ball, params);
        const Polynomial separation = horizontalDistanceQuartic(
            xy(p.c0 - cushion.center), xy(p.c1), xy(p.c2), cushion.radius + params.R);
        return firstClosingRoot(separation, 0.0, horizon);
    }

    double pocketEntryTime(const BallState& ball, const Pocket& pocket, const BallParams& params, double horizon)
    {
        if (!isMoving(ball))
        {
            return INF;
        }

        const PositionPolynomial p = positionPolynomial(ball, params);
        const Polynomial separation = horizontalDistanceQuartic(
            xy(p.c0 - pocket.center), xy(p.c1), xy(p.c2), pocket.radius);
        return firstClosingRoot(separation, 0.0, horizon);
    }

    glm::dvec3 linearCushionNormal(const BallState& ball, const LinearCushion& cushion)
    {
        const glm::dvec2 axis = glm::normalize(xy(cushion.p2 - cushion.p1));
        glm::dvec2 normal(-axis.y, axis.x);
        // Point from the ball towards the cushion.
        if (glm::dot(xy(ball.r - cushion.p1), normal) > 0.0)
        {
            normal = -normal;
        }
        return {normal.x, normal.y, 0.0};
    }

    glm::dvec3 circularCushionNormal(const BallState& ball, const CircularCushion& cushion)
    {
        const glm::dvec2 toward = glm::normalize(xy(cushion.center - ball.r));
        return {toward.x, toward.y, 0.0};
    }
}
