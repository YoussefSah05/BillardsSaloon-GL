// The referee inside a running MatchSession: ball in hand, spotting, choices,
// calls, push-outs and the race to N.

#include "gameplay/match_session.h"
#include "scene/components.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace BilliardsSaloon;

namespace
{
    constexpr double STEP = 1.0 / 120.0;
    constexpr float PI = 3.14159265358979323846f;

    MatchSettings fixedRack(int raceTo = 1)
    {
        MatchSettings settings;
        settings.raceTo = raceTo;
        settings.shuffleRack = false;
        return settings;
    }

    void aimAt(MatchSession& session, float angleRadians)
    {
        ShotControls controls;
        controls.aimDeltaRadians = angleRadians - session.shotState().aimAngleRadians;
        session.applyShotControls(controls, 0.0f);
    }

    // Charges with the keyboard to about power01, releases, and plays the shot out.
    void shoot(MatchSession& session, float power01)
    {
        ShotControls hold;
        hold.shootHeld = true;
        const ShotInputTuning tuning;
        const int frames = std::max(1, static_cast<int>(std::lround(power01 / tuning.chargePerSecond * 120.0f)));
        for (int i = 0; i < frames; ++i)
        {
            session.applyShotControls(hold, 1.0f / 120.0f);
        }
        session.applyShotControls(ShotControls{}, 1.0f / 120.0f);
        REQUIRE(session.shotState().phase == ShotPhase::BallsInMotion);
        for (double t = 0.0; (t < 60.0) && (session.shotState().phase == ShotPhase::BallsInMotion); t += STEP)
        {
            session.step(STEP);
        }
    }

    glm::vec3 position(const MatchSession& session, int number)
    {
        return session.registry().get<TransformComponent>(*session.ballEntity(number)).position;
    }

    bool pocketed(const MatchSession& session, int number)
    {
        return session.registry().get<BallComponent>(*session.ballEntity(number)).pocketed;
    }

    // A ball 15 cm from the (+x, +z) corner pocket with the cue ball 30 cm
    // behind it on the diagonal: a straight pot along 135 degrees of aim.
    struct CornerShot
    {
        glm::vec2 object;
        glm::vec2 cue;
        float aim {0.75f * PI};
    };

    CornerShot cornerShot(const GameVariantDefinition& variant)
    {
        const glm::vec2 corner(0.5f * variant.table.clothWidth, 0.5f * variant.table.clothDepth);
        const glm::vec2 diagonal = glm::normalize(glm::vec2(1.0f, 1.0f));
        CornerShot shot;
        shot.object = corner - diagonal * 0.2f;
        shot.cue = shot.object - diagonal * 0.3f;
        return shot;
    }
}

TEST_CASE("an 8-ball break that drives too few balls to a cushion offers the re-rack choice")
{
    MatchSession session(eightBallVariant(), {}, fixedRack());
    aimAt(session, -0.5f * PI);   // away from the rack, into the head cushion
    shoot(session, 0.15f);

    const Rules::Verdict& verdict = session.lastOutcome().verdict;
    CHECK(verdict.illegalBreak);
    CHECK(session.pendingChoice() == Rules::Choice::IllegalBreak);
    CHECK(session.chooser() == 1);
    CHECK_FALSE(session.acceptsShotInput());

    SUBCASE("accept the table")
    {
        CHECK(session.choose(Rules::Option::Play));
        CHECK(session.frame().shooter == 1);
        CHECK(session.acceptsShotInput());
    }
    SUBCASE("re-rack and break")
    {
        CHECK(session.choose(Rules::Option::Rerack));
        CHECK(session.frame().phase == Rules::Phase::Break);
        CHECK(session.frame().breaker == 1);
        CHECK(session.objectBallsOnTable().size() == 15);
    }
}

TEST_CASE("a foul gives ball in hand anywhere: placement is clamped, checked for overlaps, then confirmed")
{
    MatchSession session(nineBallVariant(), {}, fixedRack());
    aimAt(session, -0.5f * PI);   // a soft break the wrong way: no ball, or too few to a cushion
    shoot(session, 0.15f);

    REQUIRE(session.lastOutcome().verdict.foul != Rules::Foul::None);
    CHECK(session.frame().shooter == 1);
    CHECK(session.frame().ballInHand == Rules::BallInHand::Anywhere);
    CHECK(session.placingCueBall());
    CHECK_FALSE(session.acceptsShotInput());

    // Right onto the 1-ball: refused.
    const glm::vec3 cue = session.registry().get<TransformComponent>(session.cueBallEntity()).position;
    const glm::vec3 one = position(session, 1);
    session.moveCueBall(glm::vec2(one.x - cue.x, one.z - cue.z));
    CHECK_FALSE(session.cueBallPlacementValid());
    CHECK_FALSE(session.confirmCueBallPlacement());

    // Anywhere on the table: past the head string is fine, but not off the cloth.
    session.moveCueBall(glm::vec2(-0.3f, 5.0f));
    const glm::vec3 placed = session.registry().get<TransformComponent>(session.cueBallEntity()).position;
    CHECK(placed.z == doctest::Approx(0.5f * nineBallVariant().table.clothDepth - nineBallVariant().table.ballRadius));
    CHECK(session.cueBallPlacementValid());
    CHECK(session.confirmCueBallPlacement());
    CHECK(session.acceptsShotInput());
}

TEST_CASE("on the break the cue ball may only be placed behind the head string")
{
    MatchSession session(eightBallVariant(), {}, fixedRack());
    CHECK(session.canPlaceCueBall());
    session.beginCueBallPlacement();
    REQUIRE(session.placingCueBall());

    session.moveCueBall(glm::vec2(1.0f, 0.0f));
    const glm::vec3 cue = session.registry().get<TransformComponent>(session.cueBallEntity()).position;
    CHECK(cue.x == doctest::Approx(session.headStringX()));
    CHECK(session.confirmCueBallPlacement());
}

TEST_CASE("9-ball: potting the 9 legally wins the frame, and the race goes on")
{
    MatchSession session(nineBallVariant(), {}, fixedRack(2));
    const CornerShot shot = cornerShot(session.variant());
    session.setLayout(shot.cue, {{9, shot.object}});
    aimAt(session, shot.aim);
    shoot(session, 0.3f);

    REQUIRE(pocketed(session, 9));
    const ShotOutcome& outcome = session.lastOutcome();
    CHECK(outcome.verdict.frameOver);
    CHECK(outcome.verdict.winner == 0);
    CHECK_FALSE(outcome.matchOver);
    CHECK(session.score().frames[0] == 1);

    session.startNextFrame();
    CHECK(session.frameNumber() == 2);
    CHECK(session.frame().phase == Rules::Phase::Break);
    CHECK(session.frame().breaker == 1);
    CHECK(session.objectBallsOnTable().size() == 9);
}

TEST_CASE("9-ball: the 9 pocketed on a foul comes back to the foot spot")
{
    MatchSession session(nineBallVariant(), {}, fixedRack());
    const CornerShot shot = cornerShot(session.variant());
    // The 1 is on the table, so hitting the 9 first is a foul.
    session.setLayout(shot.cue, {{1, glm::vec2(-0.9f, -0.4f)}, {9, shot.object}});
    aimAt(session, shot.aim);
    shoot(session, 0.3f);

    const Rules::Verdict& verdict = session.lastOutcome().verdict;
    CHECK(verdict.foul == Rules::Foul::WrongBallFirst);
    CHECK(verdict.spot == std::vector<int>{9});
    CHECK_FALSE(session.frameOver());
    CHECK_FALSE(pocketed(session, 9));
    CHECK(position(session, 9).x == doctest::Approx(session.footSpot().x));
    CHECK(position(session, 9).z == doctest::Approx(0.0f));
    CHECK(session.placingCueBall());
}

TEST_CASE("8-ball: the 8 on the break is spotted and the breaker chooses")
{
    MatchSession session(eightBallVariant(), {}, fixedRack());
    const CornerShot shot = cornerShot(session.variant());
    session.setLayout(shot.cue, {{8, shot.object}});
    CHECK_FALSE(session.callRequired());   // the break needs no call

    aimAt(session, shot.aim);
    shoot(session, 0.3f);
    // 8 on the break: spotted, the breaker chooses.
    CHECK(session.lastOutcome().verdict.spot == std::vector<int>{8});
    CHECK(session.pendingChoice() == Rules::Choice::EightOnBreak);
    CHECK(session.choose(Rules::Option::Play));
    CHECK(session.frame().shooter == 0);
}

TEST_CASE("10-ball: every shot is called; the call follows the aim and can be changed by hand")
{
    MatchSession session(tenBallVariant(), {}, fixedRack());
    const CornerShot shot = cornerShot(session.variant());

    // A layout break that legally pots the 1; then check the next shot's call.
    session.setLayout(shot.cue, {{1, shot.object}, {2, glm::vec2(-0.8f, 0.0f)}, {10, glm::vec2(-0.8f, -0.4f)}});
    aimAt(session, shot.aim);
    shoot(session, 0.3f);
    REQUIRE(pocketed(session, 1));
    REQUIRE(session.lastOutcome().verdict.foul == Rules::Foul::None);
    REQUIRE(session.frame().shooter == 0);

    CHECK(session.callRequired());
    REQUIRE(session.calledShot().has_value());

    // Aim straight at the 2: it becomes the called ball.
    const glm::vec3 cue = session.registry().get<TransformComponent>(session.cueBallEntity()).position;
    const glm::vec3 two = position(session, 2);
    const glm::vec3 toTwo = glm::normalize(two - cue);
    aimAt(session, std::atan2(toTwo.x, -toTwo.z));
    CHECK(session.calledShot()->ball == 2);

    const int pocket = session.calledShot()->pocket;
    session.cycleCalledPocket(1);
    CHECK(session.calledShot()->pocket == (pocket + 1) % 6);
    session.cycleCalledBall(1);
    CHECK(session.calledShot()->ball == 10);

    // A push-out is available on the shot after the break and needs no call.
    CHECK(session.pushOutAvailable());
    session.setPushOut(true);
    CHECK(session.pushOutDeclared());
    CHECK_FALSE(session.callRequired());
}
