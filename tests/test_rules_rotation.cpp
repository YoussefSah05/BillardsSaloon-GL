// WPA 9-ball (section 5) and 10-ball (section 6), clause by clause.

#include "rules_test_helpers.h"

#include <doctest/doctest.h>

using namespace BilliardsSaloon;
using namespace RulesTest;

namespace
{
    // After a legal break with nothing down: player 1 to shoot, push-out available.
    FrameState afterBreak(GameDiscipline discipline)
    {
        FrameState state = startFrame(discipline, 0);
        const int last = (discipline == GameDiscipline::NineBall) ? 9 : 10;
        (void)judgeShot(state, balls(1, last), Shot{}.hit(1).toRail(4));
        return state;
    }

    // In play, no push-out: player 0 to shoot.
    FrameState inPlay(GameDiscipline discipline)
    {
        FrameState state = startFrame(discipline, 0);
        state.phase = Phase::Play;
        state.ballInHand = BallInHand::None;
        return state;
    }
}

TEST_CASE("9-ball 5.2: the break must hit the 1 first")
{
    FrameState state = startFrame(GameDiscipline::NineBall, 0);
    const Verdict verdict = judgeShot(state, balls(1, 9), Shot{}.hit(3).pot(5).toRail(6));

    CHECK(verdict.foul == Foul::WrongBallFirst);
    CHECK(verdict.foulsInRow == 1);
    CHECK(state.shooter == 1);
    CHECK(state.ballInHand == BallInHand::Anywhere);
    CHECK_FALSE(state.pushOutAvailable);
}

TEST_CASE("9-ball 5.2: four balls to a cushion or one pocketed")
{
    FrameState state = startFrame(GameDiscipline::NineBall, 0);
    const Verdict verdict = judgeShot(state, balls(1, 9), Shot{}.hit(1).toRail(3));
    CHECK(verdict.foul == Foul::IllegalBreak);
    CHECK(state.ballInHand == BallInHand::Anywhere);
}

TEST_CASE("9-ball 5.3: the 9 on a legal break wins")
{
    FrameState state = startFrame(GameDiscipline::NineBall, 0);
    const Verdict verdict = judgeShot(state, balls(1, 9), Shot{}.hit(1).pot(9).toRail(4));
    CHECK(verdict.frameOver);
    CHECK(verdict.winner == 0);
    CHECK(verdict.end == FrameEnd::GameBallPotted);
}

TEST_CASE("9-ball 5.3: the 9 on a foul break is spotted")
{
    FrameState state = startFrame(GameDiscipline::NineBall, 0);
    const Verdict verdict = judgeShot(state, balls(1, 9), Shot{}.hit(1).pot(9).scratch());
    CHECK_FALSE(verdict.frameOver);
    CHECK(verdict.spot == std::vector<int>{9});
    CHECK(state.shooter == 1);
}

TEST_CASE("9-ball 5.4: push-out after the break")
{
    FrameState state = afterBreak(GameDiscipline::NineBall);
    REQUIRE(state.shooter == 1);
    REQUIRE(state.pushOutAvailable);

    SUBCASE("no contact needed; the opponent chooses")
    {
        const Verdict verdict = judgeShot(state, balls(1, 9), Shot{}.push());
        CHECK(verdict.pushOut);
        CHECK(verdict.foul == Foul::None);
        CHECK(verdict.choice == Choice::AfterPushOut);
        CHECK(verdict.chooser == 0);

        SUBCASE("play from here")
        {
            CHECK(applyChoice(state, Option::Play).accepted);
            CHECK(state.shooter == 0);
        }
        SUBCASE("hand it back")
        {
            CHECK(applyChoice(state, Option::PassBack).accepted);
            CHECK(state.shooter == 1);
        }
        CHECK(state.ballInHand == BallInHand::None);
        CHECK_FALSE(state.pushOutAvailable);
    }
    SUBCASE("the 9 pocketed on a push-out is spotted")
    {
        const Verdict verdict = judgeShot(state, balls(1, 9), Shot{}.hit(4).push().pot(9));
        CHECK_FALSE(verdict.frameOver);
        CHECK(verdict.spot == std::vector<int>{9});
    }
    SUBCASE("a scratch on a push-out is a foul")
    {
        const Verdict verdict = judgeShot(state, balls(1, 9), Shot{}.push().scratch());
        CHECK(verdict.foul == Foul::CueBallPocketed);
        CHECK(verdict.choice == Choice::None);
        CHECK(state.ballInHand == BallInHand::Anywhere);
        CHECK(state.shooter == 0);
    }
    SUBCASE("only on the shot right after the break")
    {
        (void)judgeShot(state, balls(1, 9), Shot{}.hit(1).rail());
        CHECK_FALSE(state.pushOutAvailable);
        const Verdict verdict = judgeShot(state, balls(1, 9), Shot{}.push());
        CHECK_FALSE(verdict.pushOut);
        CHECK(verdict.foul == Foul::NoBallHit);
    }
}

TEST_CASE("9-ball 5.5: the lowest ball must be hit first")
{
    FrameState state = inPlay(GameDiscipline::NineBall);
    CHECK(legalFirstContacts(state, balls({3, 5, 9})) == std::vector<int>{3});
    const Verdict verdict = judgeShot(state, balls({3, 5, 9}), Shot{}.hit(5).pot(5));
    CHECK(verdict.foul == Foul::WrongBallFirst);
}

TEST_CASE("9-ball: any ball pocketed on a legal shot keeps the turn")
{
    FrameState state = inPlay(GameDiscipline::NineBall);
    const Verdict verdict = judgeShot(state, balls({3, 5, 9}), Shot{}.hit(3).pot(5));
    CHECK(verdict.foul == Foul::None);
    CHECK_FALSE(verdict.turnPassed);
}

TEST_CASE("9-ball 5.6: the 9 pocketed on any legal shot wins, combinations included")
{
    FrameState state = inPlay(GameDiscipline::NineBall);
    const Verdict verdict = judgeShot(state, balls({3, 5, 9}), Shot{}.hit(3).pot(9));
    CHECK(verdict.winner == 0);
    CHECK(verdict.end == FrameEnd::GameBallPotted);
}

TEST_CASE("9-ball: the 9 pocketed on a foul is spotted")
{
    FrameState state = inPlay(GameDiscipline::NineBall);
    const Verdict verdict = judgeShot(state, balls({3, 5, 9}), Shot{}.hit(5).pot(9));
    CHECK(verdict.foul == Foul::WrongBallFirst);
    CHECK(verdict.spot == std::vector<int>{9});
    CHECK_FALSE(verdict.frameOver);
}

TEST_CASE("9-ball 5.9: three fouls in a row lose the frame; a legal shot resets the count")
{
    FrameState state = inPlay(GameDiscipline::NineBall);
    const std::vector<int> table = balls({2, 9});
    const ShotRecord miss = Shot{};
    const ShotRecord safe = Shot{}.hit(2).rail();

    auto playerOneMisses = [&]
    {
        const Verdict verdict = judgeShot(state, table, miss);
        (void)judgeShot(state, table, safe);   // player 1 plays a legal safety
        return verdict;
    };

    CHECK(playerOneMisses().foulsInRow == 1);
    CHECK(playerOneMisses().foulsInRow == 2);

    SUBCASE("third foul")
    {
        const Verdict verdict = judgeShot(state, table, miss);
        CHECK(verdict.frameOver);
        CHECK(verdict.winner == 1);
        CHECK(verdict.end == FrameEnd::ThreeFouls);
    }
    SUBCASE("a legal shot in between")
    {
        CHECK(judgeShot(state, table, safe).foulsInRow == 0);
        (void)judgeShot(state, table, safe);
        CHECK(judgeShot(state, table, miss).foulsInRow == 1);
    }
}

TEST_CASE("10-ball 6.2: the 10 on the break is spotted and the breaker continues")
{
    FrameState state = startFrame(GameDiscipline::TenBall, 0);
    const Verdict verdict = judgeShot(state, balls(1, 10), Shot{}.hit(1).pot(10).toRail(4));
    CHECK_FALSE(verdict.frameOver);
    CHECK(verdict.spot == std::vector<int>{10});
    CHECK_FALSE(verdict.turnPassed);
    CHECK(state.pushOutAvailable);
}

TEST_CASE("10-ball 6.3: every shot after the break is called")
{
    FrameState state = startFrame(GameDiscipline::TenBall, 0);
    CHECK_FALSE(callRequired(state, balls(1, 10)));
    state.phase = Phase::Play;
    CHECK(callRequired(state, balls(1, 10)));
}

TEST_CASE("10-ball: the called ball in the called pocket keeps the turn")
{
    FrameState state = inPlay(GameDiscipline::TenBall);
    const Verdict verdict = judgeShot(state, balls({1, 4, 10}), Shot{}.hit(1).pot(4, 2).call(4, 2));
    CHECK_FALSE(verdict.turnPassed);
    CHECK(verdict.choice == Choice::None);
}

TEST_CASE("10-ball 6.4: an uncalled pot passes the turn with an option to hand it back")
{
    FrameState state = inPlay(GameDiscipline::TenBall);
    const Verdict verdict = judgeShot(state, balls({1, 4, 10}), Shot{}.hit(1).pot(4, 3).call(4, 2));
    CHECK(verdict.foul == Foul::None);
    CHECK(verdict.turnPassed);
    CHECK(verdict.choice == Choice::AfterUncalledPot);
    CHECK(verdict.chooser == 1);

    CHECK(applyChoice(state, Option::PassBack).accepted);
    CHECK(state.shooter == 0);
}

TEST_CASE("10-ball: a miss with nothing down simply passes the turn")
{
    FrameState state = inPlay(GameDiscipline::TenBall);
    const Verdict verdict = judgeShot(state, balls({1, 4, 10}), Shot{}.hit(1).rail().call(1, 0));
    CHECK(verdict.turnPassed);
    CHECK(verdict.choice == Choice::None);
}

TEST_CASE("10-ball 6.5: the 10 wins only when called")
{
    FrameState state = inPlay(GameDiscipline::TenBall);

    SUBCASE("called")
    {
        const Verdict verdict = judgeShot(state, balls({1, 10}), Shot{}.hit(1).pot(10, 4).call(10, 4));
        CHECK(verdict.winner == 0);
        CHECK(verdict.end == FrameEnd::GameBallPotted);
    }
    SUBCASE("not called: spotted")
    {
        const Verdict verdict = judgeShot(state, balls({1, 10}), Shot{}.hit(1).pot(1, 0).pot(10, 4).call(1, 0));
        CHECK_FALSE(verdict.frameOver);
        CHECK(verdict.spot == std::vector<int>{10});
        CHECK_FALSE(verdict.turnPassed);
    }
}

#include "rules/match_score.h"

TEST_CASE("race to N: alternate break and match winner")
{
    Rules::MatchScore score;
    score.raceTo = 2;

    CHECK(score.recordFrame(1, 0) == 1);   // player 1 won; breaks alternate to player 1
    CHECK(score.onTheHill(1));
    CHECK_FALSE(score.over());
    CHECK(score.recordFrame(0, 1) == 0);
    CHECK(score.recordFrame(1, 0) == 1);
    CHECK(score.over());
    CHECK(score.winner == 1);
    CHECK(score.frames == std::array<int, 2>{1, 2});
}

TEST_CASE("race to N: winner breaks")
{
    Rules::MatchScore score;
    score.raceTo = 3;
    score.order = Rules::BreakOrder::WinnerBreaks;
    CHECK(score.recordFrame(1, 0) == 1);
    CHECK(score.recordFrame(1, 1) == 1);
}
