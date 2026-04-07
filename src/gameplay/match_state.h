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

    struct PlayerMatchState
    {
        PlayerTargetGroup targetGroup {PlayerTargetGroup::None};
    };

    struct MatchState
    {
        GameDiscipline discipline {GameDiscipline::EightBall}; // For now we only support 8-ball, but this will determine the ruleset and win conditions.
        MatchFlowPhase flowPhase {MatchFlowPhase::BreakShot};
        int activePlayerIndex {0};
        bool shotInProgress {false};
        bool foulCommittedThisTurn {false};
        bool ballInHand {false};

        PlayerMatchState players[2];
    };
}