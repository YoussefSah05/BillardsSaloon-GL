#pragma once

#include "sim/motion.h"
#include "sim/table.h"

namespace BilliardsSaloon::Sim
{
    // Each detector returns the time from now until the event, or +infinity
    // if it does not happen before `horizon`. Positions are only polynomial
    // until a ball's next motion transition, so callers pass that as horizon.

    [[nodiscard]] double ballBallCollisionTime(
        const BallState& a, const BallState& b, const BallParams& params, double horizon);

    // Contact with the cushion nose, inside the segment, while approaching it.
    [[nodiscard]] double linearCushionCollisionTime(
        const BallState& ball, const LinearCushion& cushion, const BallParams& params, double horizon);

    [[nodiscard]] double circularCushionCollisionTime(
        const BallState& ball, const CircularCushion& cushion, const BallParams& params, double horizon);

    // The ball's centre enters the pocket circle.
    [[nodiscard]] double pocketEntryTime(
        const BallState& ball, const Pocket& pocket, const BallParams& params, double horizon);

    // Horizontal unit normal of a cushion at the ball, pointing from the ball
    // towards the cushion (used to resolve the impact).
    [[nodiscard]] glm::dvec3 linearCushionNormal(const BallState& ball, const LinearCushion& cushion);
    [[nodiscard]] glm::dvec3 circularCushionNormal(const BallState& ball, const CircularCushion& cushion);
}
