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
    };
}