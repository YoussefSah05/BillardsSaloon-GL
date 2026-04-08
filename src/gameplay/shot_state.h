#pragma once

namespace BilliardsSaloon
{
    enum class ShotPhase
    {
        Aiming,
        Charging,
        BallsInMotion
    };

    struct ShotState
    {
        ShotPhase phase {ShotPhase::Aiming};
        float aimAngleRadians {0.0f};
        float charge01 {0.0f};

        // Relative strike point on the cue ball:
        // strikeRight01:  -1 = left english, +1 = right english
        // strikeForward01: -1 = draw, +1 = follow
        float strikeRight01 {0.0f};
        float strikeForward01 {0.0f};
    };
}