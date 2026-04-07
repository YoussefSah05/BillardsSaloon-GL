#pragma once

#include "scene/components.h"

#include <vector>

namespace BilliardsSaloon
{
    struct PocketedBallRecord
    {
        int number {0};
        BallRuleTag ruleTag {BallRuleTag::Numbered};
        bool isCueBall {false};
    };

    struct ShotResult
    {
        bool shotActive {false};
        bool cueBallPocketed {false};

        int firstObjectBallNumber {-1};
        BallRuleTag firstObjectBallTag {BallRuleTag::Numbered};

        std::vector<PocketedBallRecord> pocketedBalls;

        void clear();

        [[nodiscard]] bool pocketedAnyObjectBall() const;
        [[nodiscard]] bool pocketedEightBall() const;
    };
}