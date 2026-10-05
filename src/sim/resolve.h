#pragma once

#include "sim/motion.h"
#include "sim/table.h"

namespace BilliardsSaloon::Sim
{
    struct CueSpecs
    {
        double M {0.567};                    // cue mass, kg
        double endMass {0.170097 / 30.0};    // effective tip mass for squirt
    };

    // A cue strike. phi: direction of travel in the table plane (degrees,
    // counter-clockwise from +x). theta: cue elevation (degrees). a, b: tip
    // contact offset as a fraction of R (a > 0 hits left of centre and gives
    // left english; b > 0 hits above centre and gives follow).
    struct CueStrike
    {
        double speed {1.0};   // cue speed at impact, m/s
        double phiDegrees {0.0};
        double thetaDegrees {0.0};
        double a {0.0};
        double b {0.0};
    };

    // Instantaneous point contact (Alciatore TP A-30) plus squirt (TP A-31).
    // Planar: vertical velocity is discarded, so no jumps yet.
    [[nodiscard]] BallState resolveStrike(const BallState& ball, const CueStrike& strike,
                                          const BallParams& params, const CueSpecs& cue = {});

    // Frictional inelastic collision of equal balls (Alciatore friction model).
    void resolveBallBall(BallState& a, BallState& b, const BallParams& params);

    // Han (2005) cushion impact, contacting the ball at the cushion height.
    // normal: horizontal unit vector from the ball towards the cushion.
    [[nodiscard]] BallState resolveCushion(const BallState& ball, const glm::dvec3& normal,
                                           double cushionHeight, const BallParams& params);

    // The ball drops into the pocket and leaves play.
    [[nodiscard]] BallState resolvePocket(const BallState& ball);

    // Gap left between bodies after an impact so it is not detected again.
    inline constexpr double MIN_DIST = 1e-6;
}
