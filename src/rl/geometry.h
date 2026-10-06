#pragma once

#include "sim/motion.h"
#include "sim/resolve.h"
#include "sim/table.h"

#include <glm/glm.hpp>

#include <optional>
#include <vector>

// Shot geometry for learning and baselines, in the simulator's frame (metres,
// origin at a corner of the playing surface, x across, y along). Theory in
// docs/design/AI.md (ghost ball, cut angle) and docs/rl/rl-system-design.md.
namespace BilliardsSaloon::Rl
{
    // Where a pocket takes a ball: the opening between its two jaw tips.
    struct PocketMouth
    {
        int pocket {0};            // index into Sim::Table::pockets
        glm::dvec2 centre {0.0};   // midpoint between the jaw tips: the aim point
        glm::dvec2 inward {0.0};   // unit vector from the mouth into the pocket
        double halfWidth {0.0};    // half the gap a ball centre can pass through
    };

    // The six mouths, indexed like the table's pockets. Each pocket's mouth
    // is spanned by its two nearest jaw tips (circular cushions).
    [[nodiscard]] std::vector<PocketMouth> pocketMouths(const Sim::Table& table);

    // The ghost-ball line for sending `object` towards `target`.
    struct GhostShot
    {
        glm::dvec2 ghost {0.0};       // cue ball centre at contact
        double phiDegrees {0.0};      // cue ball direction of travel
        double cutDegrees {0.0};      // angle between the cue ball's path and the object ball's
        double cueDistance {0.0};     // cue ball to ghost
        double objectDistance {0.0};  // object ball to target
    };

    [[nodiscard]] GhostShot ghostShot(const glm::dvec2& cue, const glm::dvec2& object, const glm::dvec2& target, double R);

    // True if a ball moving from `from` to `to` would hit a ball in
    // `positions` (other than the indices ignored).
    [[nodiscard]] bool pathBlocked(const std::vector<glm::dvec2>& positions, const glm::dvec2& from, const glm::dvec2& to,
                                   double R, int ignoreA, int ignoreB);

    // A cue speed that should bring the object ball to the target at about
    // `arrival` m/s, from rolling resistance, the slide-to-roll loss (5/7)
    // and the collision's cos(cut) transfer (centre-ball hit, no spin).
    [[nodiscard]] double cueSpeedFor(const GhostShot& shot, const Sim::BallParams& params, const Sim::CueSpecs& cue,
                                     double arrival = 0.6);

    // The geometry baseline's shot for one object ball: over the pockets
    // whose lines are clear and whose cut is below `maxCutDegrees`, the
    // easiest by cos^2(cut) exp(-(d_c + d_o) / 2.2). Empty if none.
    struct GeometryShot
    {
        int pocket {-1};
        Sim::CueStrike strike;
        GhostShot ghost;
        double ease {0.0};
    };

    [[nodiscard]] std::optional<GeometryShot> geometryShot(const Sim::Table& table, const std::vector<PocketMouth>& mouths,
                                                           const std::vector<glm::dvec2>& positions, int objectBall,
                                                           const Sim::BallParams& params, double maxCutDegrees = 75.0,
                                                           int onlyPocket = -1);

    // The ease of a ghost shot, as above (0 when the cut is too thin).
    [[nodiscard]] double shotEase(const GhostShot& shot, double maxCutDegrees = 75.0);
}
