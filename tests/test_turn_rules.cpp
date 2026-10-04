// Tests for the prototype 8-ball rules (Rules::resolveShot). They will be
// superseded by per-clause WPA tests when the RuleSet/Referee lands.

#include "gameplay/turn_rules.h"
#include "scene/components.h"

#include <doctest/doctest.h>

#include <map>

using namespace BilliardsSaloon;

namespace
{
    struct EightBallTable
    {
        Registry registry;
        std::map<int, Entity> balls;
        MatchState match;

        EightBallTable()
        {
            const GameVariantDefinition& variant = eightBallVariant();
            spawn(variant.cueBall);
            for (const BallSpawnDefinition& definition : variant.objectBalls)
            {
                spawn(definition);
            }
        }

        void spawn(const BallSpawnDefinition& definition)
        {
            const Entity entity = registry.createEntity();
            BallComponent ball;
            ball.number = definition.number;
            ball.ruleTag = definition.ruleTag;
            ball.isCueBall = definition.isCueBall;
            registry.emplace<BallComponent>(entity, ball);
            balls[definition.number] = entity;
        }

        // Marks a ball pocketed on the table and records it in the shot.
        void pocket(ShotResult& shot, int number)
        {
            BallComponent& ball = registry.get<BallComponent>(balls.at(number));
            ball.pocketed = true;
            shot.pocketedBalls.push_back({ball.number, ball.ruleTag, ball.isCueBall});
            if (ball.isCueBall)
            {
                shot.cueBallPocketed = true;
            }
        }

        void assignGroups(PlayerTargetGroup firstPlayer)
        {
            match.flowPhase = MatchFlowPhase::GroupsAssigned;
            match.players[0].targetGroup = firstPlayer;
            match.players[1].targetGroup = (firstPlayer == PlayerTargetGroup::Solids)
                ? PlayerTargetGroup::Stripes
                : PlayerTargetGroup::Solids;
        }

        void resolve(const ShotResult& shot)
        {
            Rules::resolveShot(eightBallVariant(), match, registry, shot);
        }
    };

    ShotResult firstContact(int number, BallRuleTag tag)
    {
        ShotResult shot;
        shot.firstObjectBallNumber = number;
        shot.firstObjectBallTag = tag;
        return shot;
    }
}

TEST_CASE("a legal break with nothing potted opens the table and passes the turn")
{
    EightBallTable table;
    table.resolve(firstContact(1, BallRuleTag::Solid));

    CHECK(table.match.flowPhase == MatchFlowPhase::TableOpen);
    CHECK(table.match.activePlayerIndex == 1);
    CHECK_FALSE(table.match.foulCommittedThisTurn);
}

TEST_CASE("scratching is a foul that gives the opponent ball in hand")
{
    EightBallTable table;
    ShotResult shot = firstContact(1, BallRuleTag::Solid);
    table.pocket(shot, 0);
    table.resolve(shot);

    CHECK(table.match.foulCommittedThisTurn);
    CHECK(table.match.ballInHand);
    CHECK(table.match.activePlayerIndex == 1);
}

TEST_CASE("missing every ball is a foul")
{
    EightBallTable table;
    table.match.flowPhase = MatchFlowPhase::TableOpen;
    table.resolve(ShotResult{});

    CHECK(table.match.foulCommittedThisTurn);
    CHECK(table.match.activePlayerIndex == 1);
}

TEST_CASE("potting a solid on an open table assigns groups and keeps the turn")
{
    EightBallTable table;
    table.match.flowPhase = MatchFlowPhase::TableOpen;
    ShotResult shot = firstContact(3, BallRuleTag::Solid);
    table.pocket(shot, 3);
    table.resolve(shot);

    CHECK(table.match.flowPhase == MatchFlowPhase::GroupsAssigned);
    CHECK(table.match.players[0].targetGroup == PlayerTargetGroup::Solids);
    CHECK(table.match.players[1].targetGroup == PlayerTargetGroup::Stripes);
    CHECK(table.match.activePlayerIndex == 0);
}

TEST_CASE("hitting the opponent's group first is a foul")
{
    EightBallTable table;
    table.assignGroups(PlayerTargetGroup::Solids);
    table.resolve(firstContact(9, BallRuleTag::Stripe));

    CHECK(table.match.foulCommittedThisTurn);
    CHECK(table.match.activePlayerIndex == 1);
}

TEST_CASE("potting the 8 before clearing your group loses the frame")
{
    EightBallTable table;
    table.assignGroups(PlayerTargetGroup::Solids);
    ShotResult shot = firstContact(2, BallRuleTag::Solid);
    table.pocket(shot, 8);
    table.resolve(shot);

    CHECK(table.match.flowPhase == MatchFlowPhase::FrameOver);
    CHECK(table.match.winnerPlayerIndex == 1);
}

TEST_CASE("potting the 8 legally after clearing your group wins the frame")
{
    EightBallTable table;
    table.assignGroups(PlayerTargetGroup::Solids);
    for (int number = 1; number <= 7; ++number)
    {
        table.registry.get<BallComponent>(table.balls.at(number)).pocketed = true;
    }

    ShotResult shot = firstContact(8, BallRuleTag::Eight);
    table.pocket(shot, 8);
    table.resolve(shot);

    CHECK(table.match.flowPhase == MatchFlowPhase::FrameOver);
    CHECK(table.match.winnerPlayerIndex == 0);
}

TEST_CASE("scratching while potting the 8 loses the frame")
{
    EightBallTable table;
    table.assignGroups(PlayerTargetGroup::Solids);
    for (int number = 1; number <= 7; ++number)
    {
        table.registry.get<BallComponent>(table.balls.at(number)).pocketed = true;
    }

    ShotResult shot = firstContact(8, BallRuleTag::Eight);
    table.pocket(shot, 8);
    table.pocket(shot, 0);
    table.resolve(shot);

    CHECK(table.match.winnerPlayerIndex == 1);
}
