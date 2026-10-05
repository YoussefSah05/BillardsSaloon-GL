#pragma once

#include "sim/simulate.h"

#include <glm/glm.hpp>

#include <vector>

namespace BilliardsSaloon::Audio
{
    enum class SoundKind
    {
        CueStrike,
        BallBall,
        Cushion,
        Pocket
    };

    // A sound to play during a shot: when (simulated seconds), what, how
    // loud (0..1, from the impact speed) and where (game coordinates).
    struct SoundCue
    {
        double time {0.0};
        SoundKind kind {SoundKind::BallBall};
        float intensity {0.0f};
        glm::vec3 position {0.0f};
    };

    // Reads a simulated shot's events into sound cues, in time order. Contacts
    // too soft to hear are dropped, and near-simultaneous contacts of the same
    // kind merge (resting balls nudging each other would otherwise buzz).
    [[nodiscard]] std::vector<SoundCue> planShotSounds(const Sim::ShotTrajectory& trajectory, double tableLength, double tableWidth);
}
