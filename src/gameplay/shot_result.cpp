#include "gameplay/shot_result.h"

namespace BilliardsSaloon
{
    void ShotResult::clear()
    {
        shotActive = false;
        cueBallPocketed = false;
        firstObjectBallNumber = -1;
        firstObjectBallTag = BallRuleTag::Numbered;
        pocketedBalls.clear();
    }

    bool ShotResult::pocketedAnyObjectBall() const
    {
        for (const PocketedBallRecord& record : pocketedBalls)
        {
            if (!record.isCueBall)
            {
                return true;
            }
        }

        return false;
    }

    bool ShotResult::pocketedEightBall() const
    {
        for (const PocketedBallRecord& record : pocketedBalls)
        {
            if (record.ruleTag == BallRuleTag::Eight)
            {
                return true;
            }
        }

        return false;
    }
}