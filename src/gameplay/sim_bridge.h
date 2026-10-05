#pragma once

#include "sim/motion.h"

#include <glm/glm.hpp>

namespace BilliardsSaloon::SimBridge
{
    // Game frame: y up, origin at the table centre, x along the length.
    // Simulator frame (pooltool's): z up, origin at a corner of the playing
    // surface, y along the length, x across it. The mapping is a rotation
    // (sim x → game z, sim y → game x, sim z → game y), so it also maps
    // angular velocities.

    [[nodiscard]] inline glm::dvec3 toSimVector(const glm::vec3& game)
    {
        return {game.z, game.x, game.y};
    }

    [[nodiscard]] inline glm::vec3 toGameVector(const glm::dvec3& sim)
    {
        return glm::vec3(static_cast<float>(sim.y), static_cast<float>(sim.z), static_cast<float>(sim.x));
    }

    [[nodiscard]] inline glm::dvec3 toSimPosition(const glm::vec3& game, double length, double width)
    {
        return toSimVector(game) + glm::dvec3(0.5 * width, 0.5 * length, 0.0);
    }

    [[nodiscard]] inline glm::vec3 toGamePosition(const glm::dvec3& sim, double length, double width)
    {
        return toGameVector(sim - glm::dvec3(0.5 * width, 0.5 * length, 0.0));
    }

    // Direction of travel for the simulator's cue strike (degrees).
    [[nodiscard]] inline double aimToPhiDegrees(const glm::vec3& gameDirection)
    {
        const glm::dvec3 sim = toSimVector(gameDirection);
        return glm::degrees(std::atan2(sim.y, sim.x));
    }
}
