// WPA 8-ball, clause by clause (World Standardized Rules, section 3).

#include "rules_test_helpers.h"

#include <doctest/doctest.h>

using namespace BilliardsSaloon;
using namespace RulesTest;

namespace
{
    // A frame after a legal break: open table, player 0 to shoot.
    FrameState openTable()
    {
        FrameState state = startFrame(GameDiscipline::EightBall, 0);
        state.phase = Phase::Open;
        state.ballInHand = BallInHand::None;
        return state;
    }

    // Groups decided: player 0 solids, player 1 stripes; player 0 to shoot.
    FrameState solidsToShoot()
    {
        FrameState state = openTable();
        state.phase = Phase::Groups;
        state.groups = {Group::Solids, Group::Stripes};
        return state;
    }
}

TEST_CASE("8-ball: a fresh frame breaks from behind the head string")
{
    const FrameState state = startFrame(GameDiscipline::EightBall, 1);
    CHECK(state.phase == Phase::Break);
    CHECK(state.shooter == 1);
    CHECK(state.breaker == 1);
    CHECK(state.ballInHand == BallInHand::BehindHeadString);
}

TEST_CASE("8-ball 3.4: the table is open after a break, even when balls drop")
{
    FrameState state = startFrame(GameDiscipline::EightBall, 0);
    const Verdict verdict = judgeShot(state, balls(1, 15), Shot{}.hit(1).pot(3).pot(11).toRail(5));

    CHECK(verdict.breakShot);
    CHECK(verdict.foul == Foul::None);
    CHECK_FALSE(verdict.turnPassed);
    CHECK_FALSE(verdict.groupsAssigned);
    CHECK(state.phase == Phase::Open);
    CHECK(state.groups[0] == Group::None);
    CHECK(state.shooter == 0);
}

TEST_CASE("8-ball 3.5: a legal break with nothing down passes the turn")
{
    FrameState state = startFrame(GameDiscipline::EightBall, 0);
    const Verdict verdict = judgeShot(state, balls(1, 15), Shot{}.hit(1).toRail(4));

    CHECK_FALSE(verdict.illegalBreak);
    CHECK(verdict.turnPassed);
    CHECK(state.shooter == 1);
    CHECK(state.choice == Choice::None);
}

TEST_CASE("8-ball 3.5: an illegal break gives the incoming player the choice")
{
    FrameState state = startFrame(GameDiscipline::EightBall, 0);
    const Verdict verdict = judgeShot(state, balls(1, 15), Shot{}.hit(1).toRail(3));

    CHECK(verdict.illegalBreak);
    CHECK(verdict.foul == Foul::None);
    CHECK(verdict.choice == Choice::IllegalBreak);
    CHECK(verdict.chooser == 1);
    CHECK(optionsFor(Choice::IllegalBreak).size() == 3);

    SUBCASE("accept the table")
    {
        const ChoiceResult result = applyChoice(state, Option::Play);
        CHECK(result.accepted);
        CHECK_FALSE(result.rerack);
        CHECK(state.shooter == 1);
        CHECK(state.phase == Phase::Open);
        CHECK(state.choice == Choice::None);
    }
    SUBCASE("re-rack and break")
    {
        const ChoiceResult result = applyChoice(state, Option::Rerack);
        CHECK(result.rerack);
        CHECK(state.phase == Phase::Break);
        CHECK(state.breaker == 1);
        CHECK(state.shooter == 1);
    }
    SUBCASE("re-rack and let the offender break again")
    {
        const ChoiceResult result = applyChoice(state, Option::RerackOpponent);
        CHECK(result.rerack);
        CHECK(state.breaker == 0);
    }
    SUBCASE("an option that is not offered is refused")
    {
        CHECK_FALSE(applyChoice(state, Option::PassBack).accepted);
        CHECK(state.choice == Choice::IllegalBreak);
    }
}

TEST_CASE("8-ball 3.6: the 8 on the break is spotted and the breaker chooses")
{
    FrameState state = startFrame(GameDiscipline::EightBall, 0);
    const Verdict verdict = judgeShot(state, balls(1, 15), Shot{}.hit(1).pot(8).toRail(4));

    CHECK_FALSE(verdict.frameOver);
    CHECK(verdict.spot == std::vector<int>{8});
    CHECK(verdict.choice == Choice::EightOnBreak);
    CHECK(verdict.chooser == 0);

    CHECK(applyChoice(state, Option::Play).accepted);
    CHECK(state.shooter == 0);
    CHECK(state.ballInHand == BallInHand::None);
}

TEST_CASE("8-ball 3.6: the 8 and the cue ball on the break: the incoming player chooses, ball in hand behind the head string")
{
    FrameState state = startFrame(GameDiscipline::EightBall, 0);
    const Verdict verdict = judgeShot(state, balls(1, 15), Shot{}.hit(1).pot(8).scratch());

    CHECK(verdict.foul == Foul::CueBallPocketed);
    CHECK(verdict.chooser == 1);
    CHECK(state.ballInHand == BallInHand::BehindHeadString);

    CHECK(applyChoice(state, Option::Play).accepted);
    CHECK(state.shooter == 1);
    CHECK(state.ballInHand == BallInHand::BehindHeadString);
}

TEST_CASE("8-ball 3.7: a scratch on a legal break is ball in hand behind the head string")
{
    FrameState state = startFrame(GameDiscipline::EightBall, 0);
    const Verdict verdict = judgeShot(state, balls(1, 15), Shot{}.hit(1).pot(2).scratch());

    CHECK(verdict.foul == Foul::CueBallPocketed);
    CHECK(verdict.turnPassed);
    CHECK(state.shooter == 1);
    CHECK(state.ballInHand == BallInHand::BehindHeadString);
    CHECK(state.phase == Phase::Open);
}

TEST_CASE("8-ball 3.4: on an open table the first ball down decides the groups")
{
    FrameState state = openTable();
    const Verdict verdict = judgeShot(state, balls(1, 15), Shot{}.hit(12).pot(12, 3).pot(2, 4));

    CHECK(verdict.groupsAssigned);
    CHECK_FALSE(verdict.turnPassed);
    CHECK(state.phase == Phase::Groups);
    CHECK(state.groups[0] == Group::Stripes);
    CHECK(state.groups[1] == Group::Solids);
}

TEST_CASE("8-ball: on an open table either group may be hit first, but not the 8")
{
    FrameState state = openTable();
    CHECK(legalFirstContacts(state, balls(1, 15)).size() == 14);

    const Verdict verdict = judgeShot(state, balls(1, 15), Shot{}.hit(8).rail());
    CHECK(verdict.foul == Foul::WrongBallFirst);
    CHECK(state.ballInHand == BallInHand::Anywhere);
    CHECK(state.shooter == 1);
}

TEST_CASE("8-ball: an open-table miss passes the turn and keeps the table open")
{
    FrameState state = openTable();
    const Verdict verdict = judgeShot(state, balls(1, 15), Shot{}.hit(4).rail());

    CHECK(verdict.foul == Foul::None);
    CHECK(verdict.turnPassed);
    CHECK(state.phase == Phase::Open);
}

TEST_CASE("8-ball 6.x fouls: ball in hand anywhere")
{
    FrameState state = solidsToShoot();

    SUBCASE("cue ball pocketed")
    {
        CHECK(judgeShot(state, balls(1, 15), Shot{}.hit(2).pot(2).scratch()).foul == Foul::CueBallPocketed);
    }
    SUBCASE("no ball hit")
    {
        CHECK(judgeShot(state, balls(1, 15), Shot{}).foul == Foul::NoBallHit);
    }
    SUBCASE("opponent's ball first")
    {
        CHECK(judgeShot(state, balls(1, 15), Shot{}.hit(10).pot(2)).foul == Foul::WrongBallFirst);
    }
    SUBCASE("no cushion after contact and nothing down")
    {
        CHECK(judgeShot(state, balls(1, 15), Shot{}.hit(3)).foul == Foul::NoRail);
    }

    CHECK(state.ballInHand == BallInHand::Anywhere);
    CHECK(state.shooter == 1);
}

TEST_CASE("8-ball: the shooter continues only after pocketing a ball of their group")
{
    FrameState state = solidsToShoot();
    Verdict verdict = judgeShot(state, balls(1, 15), Shot{}.hit(3).pot(3));
    CHECK_FALSE(verdict.turnPassed);
    CHECK(state.shooter == 0);

    verdict = judgeShot(state, balls({1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}), Shot{}.hit(1).pot(10).rail());
    CHECK(verdict.foul == Foul::None);
    CHECK(verdict.turnPassed);
    CHECK(state.shooter == 1);
}

TEST_CASE("8-ball: the 8 must be called once the group is cleared")
{
    FrameState state = solidsToShoot();
    CHECK_FALSE(callRequired(state, balls(1, 15)));
    CHECK(callRequired(state, balls({8, 9, 12})));
    CHECK(legalFirstContacts(state, balls({8, 9, 12})) == std::vector<int>{8});
}

TEST_CASE("8-ball 3.10: the 8 decides the frame")
{
    FrameState state = solidsToShoot();

    SUBCASE("called and pocketed after clearing the group: shooter wins")
    {
        const Verdict verdict = judgeShot(state, balls({8, 9}), Shot{}.hit(8).pot(8, 5).call(8, 5));
        CHECK(verdict.frameOver);
        CHECK(verdict.winner == 0);
        CHECK(verdict.end == FrameEnd::GameBallPotted);
    }
    SUBCASE("in a pocket that was not called: shooter loses")
    {
        const Verdict verdict = judgeShot(state, balls({8, 9}), Shot{}.hit(8).pot(8, 2).call(8, 5));
        CHECK(verdict.winner == 1);
        CHECK(verdict.end == FrameEnd::EightBallWrongPocket);
    }
    SUBCASE("with a scratch: shooter loses")
    {
        const Verdict verdict = judgeShot(state, balls({8, 9}), Shot{}.hit(8).pot(8, 5).call(8, 5).scratch(1));
        CHECK(verdict.winner == 1);
        CHECK(verdict.end == FrameEnd::EightBallOnFoul);
    }
    SUBCASE("before the group is cleared: shooter loses")
    {
        const Verdict verdict = judgeShot(state, balls({2, 8, 9}), Shot{}.hit(2).pot(8, 5));
        CHECK(verdict.winner == 1);
        CHECK(verdict.end == FrameEnd::EightBallEarly);
    }
    SUBCASE("on an open table: shooter loses")
    {
        state = openTable();
        const Verdict verdict = judgeShot(state, balls(1, 15), Shot{}.hit(2).pot(8));
        CHECK(verdict.end == FrameEnd::EightBallEarly);
        CHECK(verdict.winner == 1);
    }

    CHECK(state.phase == Phase::FrameOver);
}

TEST_CASE("8-ball: no three-foul rule")
{
    FrameState state = solidsToShoot();
    for (int i = 0; i < 4; ++i)
    {
        const Verdict verdict = judgeShot(state, balls(1, 15), Shot{});
        CHECK(verdict.foul == Foul::NoBallHit);
        CHECK_FALSE(verdict.frameOver);
    }
}

TEST_CASE("referee ignores shots while a choice is pending or after the frame")
{
    FrameState state = startFrame(GameDiscipline::EightBall, 0);
    (void)judgeShot(state, balls(1, 15), Shot{}.hit(1).toRail(2));
    REQUIRE(state.choice == Choice::IllegalBreak);

    const FrameState before = state;
    (void)judgeShot(state, balls(1, 15), Shot{}.hit(1).pot(1));
    CHECK(state.shooter == before.shooter);
    CHECK(state.choice == Choice::IllegalBreak);
}
