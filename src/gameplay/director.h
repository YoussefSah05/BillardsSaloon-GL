#pragma once

#include "sim/simulate.h"

#include <glm/glm.hpp>

#include <vector>

namespace BilliardsSaloon
{
    enum class DirectorShot
    {
        Player,     // the player's own camera (aim or follow)
        Wide,       // the high wide view of the whole table
        Pocket      // low behind a pocket, looking at the ball coming in
    };

    struct DirectorCut
    {
        double time {0.0};          // simulated seconds into the shot
        DirectorShot shot {DirectorShot::Player};
        glm::vec3 position {0.0f};  // Pocket only: camera position and target
        glm::vec3 target {0.0f};
    };

    struct DirectorTable
    {
        float length {2.54f};       // game x
        float width {1.27f};        // game z
        float ballRadius {0.028575f};
        std::vector<glm::vec3> pockets;   // game coordinates, simulator pocket order
    };

    // Plans broadcast cuts for a shot whose whole future is known: a pocket
    // camera shortly before the first object ball drops, else a wide view
    // for long shots. Cuts are in time order and start with Player at 0.
    [[nodiscard]] std::vector<DirectorCut> planShotCoverage(
        const Sim::ShotTrajectory& trajectory,
        const DirectorTable& table);

    // The cut in force at time t.
    [[nodiscard]] const DirectorCut& cutAt(const std::vector<DirectorCut>& cuts, double t);
}
