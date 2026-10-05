#pragma once

#include "gameplay/game_variant.h"

namespace BilliardsSaloon
{
    enum class MatchFlowPhase
    {
        BreakShot,
        TableOpen,
        GroupsAssigned,
        FrameOver
    };

    enum class PlayerTargetGroup
    {
        None,
        Solids,
        Stripes
    };

    // Why the last shot was a foul, so the referee can say so.
    enum class FoulReason
    {
        None,
        CueBallPocketed,
        NoBallHit,
        WrongBallFirst
    };

    // How the frame ended, when it has.
    enum class FrameEndReason
    {
        None,
        EightBallPotted,          // legally, after clearing the group: shooter wins
        EightBallPottedEarly,     // before clearing the group: shooter loses
        EightBallPottedOnFoul     // on the same shot as a foul: shooter loses
    };

    struct PlayerMatchState
    {
        PlayerTargetGroup targetGroup {PlayerTargetGroup::None};
    };

    struct MatchState
    {
        GameDiscipline discipline {GameDiscipline::EightBall};
        MatchFlowPhase flowPhase {MatchFlowPhase::BreakShot};
        int activePlayerIndex {0};
        int winnerPlayerIndex {-1};

        bool shotInProgress {false};
        bool foulCommittedThisTurn {false};
        bool ballInHand {false};
        FoulReason lastFoul {FoulReason::None};
        FrameEndReason frameEnd {FrameEndReason::None};

        PlayerMatchState players[2];
    };
}