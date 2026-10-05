#include "gameplay/turn_rules.h"

#include "scene/components.h"

#include <optional>

namespace BilliardsSaloon
{
    namespace Rules
    {
        namespace
        {
            [[nodiscard]] int otherPlayerIndex(int index)
            {
                return (index == 0) ? 1 : 0;
            }

            [[nodiscard]] bool tagMatchesGroup(BallRuleTag tag, PlayerTargetGroup group)
            {
                switch (group)
                {
                    case PlayerTargetGroup::Solids:
                        return tag == BallRuleTag::Solid;
                    case PlayerTargetGroup::Stripes:
                        return tag == BallRuleTag::Stripe;
                    case PlayerTargetGroup::None:
                        return false;
                }

                return false;
            }

            [[nodiscard]] bool shotPocketedGroup(
                const ShotResult& shotResult,
                PlayerTargetGroup group)
            {
                for (const PocketedBallRecord& record : shotResult.pocketedBalls)
                {
                    if (tagMatchesGroup(record.ruleTag, group))
                    {
                        return true;
                    }
                }

                return false;
            }

            [[nodiscard]] std::optional<PlayerTargetGroup> firstGroupAssignedFromShot(
                const ShotResult& shotResult)
            {
                for (const PocketedBallRecord& record : shotResult.pocketedBalls)
                {
                    if (record.ruleTag == BallRuleTag::Solid)
                    {
                        return PlayerTargetGroup::Solids;
                    }

                    if (record.ruleTag == BallRuleTag::Stripe)
                    {
                        return PlayerTargetGroup::Stripes;
                    }
                }

                return std::nullopt;
            }

            [[nodiscard]] int remainingBallsForGroup(
                Registry& registry,
                PlayerTargetGroup group)
            {
                int count = 0;

                registry.view<BallComponent>().each(
                    [&](Entity, BallComponent& ball)
                    {
                        if (ball.pocketed)
                        {
                            return;
                        }

                        if (tagMatchesGroup(ball.ruleTag, group))
                        {
                            ++count;
                        }
                    }
                );

                return count;
            }

            [[nodiscard]] std::optional<BallRuleTag> requiredFirstContactTag(
                const MatchState& matchState,
                Registry& registry)
            {
                if (matchState.flowPhase != MatchFlowPhase::GroupsAssigned)
                {
                    return std::nullopt;
                }

                const PlayerTargetGroup activeGroup =
                    matchState.players[matchState.activePlayerIndex].targetGroup;

                if (activeGroup == PlayerTargetGroup::None)
                {
                    return std::nullopt;
                }

                if (remainingBallsForGroup(registry, activeGroup) == 0)
                {
                    return BallRuleTag::Eight;
                }

                return (activeGroup == PlayerTargetGroup::Solids)
                    ? BallRuleTag::Solid
                    : BallRuleTag::Stripe;
            }

            void resolveEightBallShot(
                MatchState& matchState,
                Registry& registry,
                const ShotResult& shotResult)
            {
                if (matchState.flowPhase == MatchFlowPhase::FrameOver)
                {
                    return;
                }

                const int activePlayer = matchState.activePlayerIndex;
                const int otherPlayer = otherPlayerIndex(activePlayer);

                bool continueTurn = false;

                const std::optional<BallRuleTag> requiredTag =
                    requiredFirstContactTag(matchState, registry);

                // When several fouls happen on one shot, report the most visible one.
                FoulReason foulReason = FoulReason::None;
                if (shotResult.cueBallPocketed)
                {
                    foulReason = FoulReason::CueBallPocketed;
                }
                else if (shotResult.firstObjectBallNumber < 0)
                {
                    foulReason = FoulReason::NoBallHit;
                }
                else if (requiredTag.has_value() && (shotResult.firstObjectBallTag != *requiredTag))
                {
                    foulReason = FoulReason::WrongBallFirst;
                }

                const bool foul = foulReason != FoulReason::None;
                matchState.lastFoul = foulReason;
                matchState.foulCommittedThisTurn = foul;
                matchState.ballInHand = foul;

                if (shotResult.pocketedEightBall())
                {
                    bool legalEightBall = false;

                    const PlayerTargetGroup targetGroup =
                        matchState.players[activePlayer].targetGroup;

                    if (!foul &&
                        (targetGroup != PlayerTargetGroup::None) &&
                        (remainingBallsForGroup(registry, targetGroup) == 0))
                    {
                        legalEightBall = true;
                    }

                    matchState.flowPhase = MatchFlowPhase::FrameOver;
                    matchState.winnerPlayerIndex = legalEightBall ? activePlayer : otherPlayer;
                    matchState.frameEnd =
                        legalEightBall ? FrameEndReason::EightBallPotted
                        : foul ? FrameEndReason::EightBallPottedOnFoul
                        : FrameEndReason::EightBallPottedEarly;
                    return;
                }

                if (matchState.flowPhase == MatchFlowPhase::BreakShot)
                {
                    matchState.flowPhase = MatchFlowPhase::TableOpen;
                }

                if (matchState.flowPhase == MatchFlowPhase::TableOpen)
                {
                    const std::optional<PlayerTargetGroup> assignedGroup =
                        firstGroupAssignedFromShot(shotResult);

                    if (!foul && assignedGroup.has_value())
                    {
                        matchState.players[activePlayer].targetGroup = *assignedGroup;
                        matchState.players[otherPlayer].targetGroup =
                            (*assignedGroup == PlayerTargetGroup::Solids)
                                ? PlayerTargetGroup::Stripes
                                : PlayerTargetGroup::Solids;

                        matchState.flowPhase = MatchFlowPhase::GroupsAssigned;
                        continueTurn = true;
                    }
                    else
                    {
                        continueTurn = !foul && shotResult.pocketedAnyObjectBall();
                    }
                }
                else if (matchState.flowPhase == MatchFlowPhase::GroupsAssigned)
                {
                    continueTurn =
                        !foul &&
                        shotPocketedGroup(
                            shotResult,
                            matchState.players[activePlayer].targetGroup
                        );
                }

                if (!continueTurn)
                {
                    matchState.activePlayerIndex = otherPlayer;
                }
            }
        }

        void resolveShot(
            const GameVariantDefinition& variant,
            MatchState& matchState,
            Registry& registry,
            const ShotResult& shotResult)
        {
            matchState.shotInProgress = false;

            switch (variant.discipline)
            {
                case GameDiscipline::EightBall:
                    resolveEightBallShot(matchState, registry, shotResult);
                    break;

                case GameDiscipline::NineBall:
                    resolveEightBallShot(matchState, registry, shotResult);
                    break;
            }
        }
    }
}