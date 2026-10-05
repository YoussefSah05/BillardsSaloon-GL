#pragma once

namespace BilliardsSaloon
{
    enum class ShotPhase
    {
        PlacingCueBall,   // ball in hand: moving the cue ball before aiming
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

        // Cue elevation above the cloth: 0 = level; higher curves the cue
        // ball when struck off centre (swerve, masse).
        float elevationDegrees {0.0f};
    };
}
