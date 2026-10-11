#pragma once

#include "sim/simulate.h"

#include <glm/glm.hpp>

#include <vector>

namespace Septhuit::Audio
{
    enum class SoundKind
    {
        CueStrike,
        BallBall,
        Cushion,
        Pocket,
        PocketJaw
    };

    // A sound to play during a shot: when (simulated seconds), what, how
    // loud (0..1, from the impact speed) and where (game coordinates).
    struct SoundCue
    {
        double time {0.0};
        SoundKind kind {SoundKind::BallBall};
        float intensity {0.0f};
        glm::vec3 position {0.0f};
        int ball {-1};
        int other {-1};
    };

    // Reads a simulated shot's events into sound cues, in time order. Contacts
    // too soft to hear are dropped; only repeated contacts at the same place
    // between the same participants merge. Distinct rack contacts stay audible.
    [[nodiscard]] std::vector<SoundCue> planShotSounds(const Sim::ShotTrajectory& trajectory, const Sim::Table& table);
}
