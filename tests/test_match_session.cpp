#include "gameplay/match_session.h"
#include "scene/components.h"

#include <doctest/doctest.h>

#include <glm/glm.hpp>

using namespace BilliardsSaloon;

namespace
{
    constexpr double STEP = 1.0 / 120.0;

    ShotControls holdShoot()
    {
        ShotControls controls;
        controls.shootHeld = true;
        return controls;
    }

    // Holds the shoot button for chargeSeconds at the given frame rate, then releases.
    void chargeAndRelease(MatchSession& session, float chargeSeconds, float framesPerSecond)
    {
        const float dt = 1.0f / framesPerSecond;
        const int frames = static_cast<int>(chargeSeconds * framesPerSecond + 0.5f);
        for (int i = 0; i < frames; ++i)
        {
            session.applyShotControls(holdShoot(), dt);
        }
        session.applyShotControls(ShotControls{}, dt);
    }

    void stepUntilSettled(MatchSession& session, double maxSeconds = 30.0)
    {
        for (double t = 0.0; (t < maxSeconds) && (session.shotState().phase == ShotPhase::BallsInMotion); t += STEP)
        {
            session.step(STEP);
        }
    }
}

TEST_CASE("a new session racks the balls and waits for the break")
{
    MatchSession session(eightBallVariant());

    CHECK(session.objectBallEntities().size() == 15);
    CHECK(session.frame().phase == Rules::Phase::Break);
    CHECK(session.frame().ballInHand == Rules::BallInHand::BehindHeadString);
    CHECK(session.shotState().phase == ShotPhase::Aiming);
    CHECK(session.acceptsShotInput());
    CHECK_FALSE(session.ballsInMotion());

    const glm::vec3 cueBall = session.registry().get<TransformComponent>(session.cueBallEntity()).position;
    CHECK(cueBall.z == doctest::Approx(session.cueBallStartPosition().z));
}

TEST_CASE("charging power does not depend on the frame rate")
{
    MatchSession slow(eightBallVariant());
    MatchSession fast(eightBallVariant());

    for (int i = 0; i < 30; ++i)
    {
        slow.applyShotControls(holdShoot(), 1.0f / 60.0f);
    }
    for (int i = 0; i < 72; ++i)
    {
        fast.applyShotControls(holdShoot(), 1.0f / 144.0f);
    }

    CHECK(slow.shotState().phase == ShotPhase::Charging);
    CHECK(slow.shotState().charge01 == doctest::Approx(0.45f).epsilon(0.01));
    CHECK(fast.shotState().charge01 == doctest::Approx(slow.shotState().charge01).epsilon(0.01));
}

TEST_CASE("aiming turns at a fixed rate and the tip offset stays inside the cue ball")
{
    MatchSession session(eightBallVariant());
    const float startAngle = session.shotState().aimAngleRadians;

    ShotControls controls;
    controls.aimAxis = 1.0f;
    controls.strikeRightAxis = 1.0f;
    controls.strikeForwardAxis = 1.0f;
    for (int i = 0; i < 120; ++i)
    {
        session.applyShotControls(controls, 1.0f / 60.0f);
    }

    const ShotInputTuning tuning;
    CHECK(session.shotState().aimAngleRadians - startAngle == doctest::Approx(2.0f * tuning.aimRadiansPerSecond));

    const glm::vec2 strike(session.shotState().strikeRight01, session.shotState().strikeForward01);
    CHECK(glm::length(strike) == doctest::Approx(tuning.maxStrikeRadius01));

    controls = ShotControls{};
    controls.centerStrike = true;
    session.applyShotControls(controls, 1.0f / 60.0f);
    CHECK(session.shotState().strikeRight01 == 0.0f);
    CHECK(session.shotState().strikeForward01 == 0.0f);
}

TEST_CASE("releasing the shoot button fires the cue ball along the aim line")
{
    MatchSession session(eightBallVariant());
    chargeAndRelease(session, 0.5f, 60.0f);

    REQUIRE(session.shotState().phase == ShotPhase::BallsInMotion);
    CHECK(session.shotState().charge01 == 0.0f);
    CHECK_FALSE(session.acceptsShotInput());

    const glm::vec3 velocity = session.registry().get<BallComponent>(session.cueBallEntity()).linearVelocity;
    CHECK(glm::length(velocity) > 1.0f);
    CHECK(glm::dot(glm::normalize(velocity), session.aimDirection()) == doctest::Approx(1.0f));
}

TEST_CASE("the break resolves once the balls stop and play continues")
{
    MatchSession session(eightBallVariant());
    chargeAndRelease(session, 1.2f, 60.0f);
    stepUntilSettled(session);

    CHECK_FALSE(session.ballsInMotion());
    CHECK(session.frame().phase != Rules::Phase::Break);

    REQUIRE(session.resolvedShotCount() == 1);
    const ShotOutcome& outcome = session.lastOutcome();
    CHECK(outcome.verdict.shooter == 0);
    CHECK(outcome.verdict.breakShot);
    CHECK(outcome.nextPlayer == session.frame().shooter);
    CHECK(outcome.verdict.turnPassed == (outcome.nextPlayer != 0));

    if (!session.frameOver() && (session.pendingChoice() == Rules::Choice::None))
    {
        CHECK((session.acceptsShotInput() || session.placingCueBall()));
        const BallComponent& cueBall = session.registry().get<BallComponent>(session.cueBallEntity());
        CHECK_FALSE(cueBall.pocketed);
    }
}

TEST_CASE("pausing while charging abandons the shot")
{
    MatchSession session(eightBallVariant());
    for (int i = 0; i < 20; ++i)
    {
        session.applyShotControls(holdShoot(), 1.0f / 60.0f);
    }
    session.cancelHeldShot();

    CHECK(session.shotState().phase == ShotPhase::Aiming);
    CHECK(session.shotState().charge01 == 0.0f);

    // Releasing after resuming must not fire.
    session.applyShotControls(ShotControls{}, 1.0f / 60.0f);
    CHECK(session.shotState().phase == ShotPhase::Aiming);
    CHECK_FALSE(session.ballsInMotion());
}

TEST_CASE("mouse aim and spin deltas apply directly")
{
    MatchSession session(eightBallVariant());
    const float startAngle = session.shotState().aimAngleRadians;

    ShotControls controls;
    controls.aimDeltaRadians = 0.25f;
    controls.strikeDelta = glm::vec2(0.2f, -0.3f);
    session.applyShotControls(controls, 1.0f / 60.0f);

    CHECK(session.shotState().aimAngleRadians - startAngle == doctest::Approx(0.25f));
    CHECK(session.shotState().strikeRight01 == doctest::Approx(0.2f));
    CHECK(session.shotState().strikeForward01 == doctest::Approx(-0.3f));
}

TEST_CASE("a mouse stroke sets power by drag distance and shoots on release")
{
    MatchSession session(eightBallVariant());

    ShotControls stroke;
    stroke.strokeHeld = true;
    stroke.strokeDelta = 0.4f;
    session.applyShotControls(stroke, 1.0f / 60.0f);
    stroke.strokeDelta = 0.4f;
    session.applyShotControls(stroke, 1.0f / 60.0f);
    stroke.strokeDelta = -0.2f;   // pushing forward again reduces power
    session.applyShotControls(stroke, 1.0f / 60.0f);

    CHECK(session.shotState().phase == ShotPhase::Charging);
    CHECK(session.shotState().charge01 == doctest::Approx(0.6f));

    // Time held does not add power for a stroke.
    stroke.strokeDelta = 0.0f;
    for (int i = 0; i < 60; ++i)
    {
        session.applyShotControls(stroke, 1.0f / 60.0f);
    }
    CHECK(session.shotState().charge01 == doctest::Approx(0.6f));

    session.applyShotControls(ShotControls{}, 1.0f / 60.0f);
    CHECK(session.shotState().phase == ShotPhase::BallsInMotion);
}

TEST_CASE("releasing a mouse stroke with almost no power cancels it")
{
    MatchSession session(eightBallVariant());

    ShotControls stroke;
    stroke.strokeHeld = true;
    stroke.strokeDelta = 0.3f;
    session.applyShotControls(stroke, 1.0f / 60.0f);
    stroke.strokeDelta = -0.29f;
    session.applyShotControls(stroke, 1.0f / 60.0f);

    session.applyShotControls(ShotControls{}, 1.0f / 60.0f);
    CHECK(session.shotState().phase == ShotPhase::Aiming);
    CHECK(session.shotState().charge01 == 0.0f);
    CHECK_FALSE(session.ballsInMotion());
}

TEST_CASE("restarting the frame restores the opening position")
{
    MatchSession session(eightBallVariant());
    chargeAndRelease(session, 1.0f, 60.0f);
    stepUntilSettled(session);

    session.restartFrame();

    CHECK(session.frame().phase == Rules::Phase::Break);
    CHECK(session.frame().shooter == 0);
    CHECK(session.frameNumber() == 1);
    for (const Entity ball : session.objectBallEntities())
    {
        CHECK_FALSE(session.registry().get<BallComponent>(ball).pocketed);
    }
}

TEST_CASE("the opening aim points from the head spot at the rack")
{
    MatchSession session(eightBallVariant());
    const glm::vec3 aim = session.aimDirection();
    CHECK(aim.x == doctest::Approx(1.0f));
    CHECK(session.cueBallStartPosition().x == doctest::Approx(-0.25f * eightBallVariant().table.clothWidth));
}

TEST_CASE("an event-simulated break plays back, syncs pots and records first contact")
{
    MatchSession session(eightBallVariant());
    REQUIRE(session.backend() == PhysicsBackend::EventBased);

    chargeAndRelease(session, 1.0f, 60.0f);
    REQUIRE(session.activeTrajectory() != nullptr);
    const std::size_t potted = [&]()
    {
        std::size_t count = 0;
        for (const auto& event : session.activeTrajectory()->events)
        {
            count += (event.type == BilliardsSaloon::Sim::EventType::Pocket) ? 1U : 0U;
        }
        return count;
    }();

    stepUntilSettled(session);
    REQUIRE(session.resolvedShotCount() == 1);
    CHECK(session.activeTrajectory() == nullptr);

    // The break hits the apex ball (the 1) first.
    CHECK(session.lastOutcome().verdict.foul != Rules::Foul::NoBallHit);

    std::size_t pocketedOnTable = 0;
    for (const Entity ball : session.objectBallEntities())
    {
        pocketedOnTable += session.registry().get<BallComponent>(ball).pocketed ? 1U : 0U;
    }
    const bool cuePotted = session.registry().get<BallComponent>(session.cueBallEntity()).pocketed ||
                           session.lastOutcome().verdict.foul == Rules::Foul::CueBallPocketed;
    CHECK(pocketedOnTable + (cuePotted ? 1U : 0U) == potted);
}

TEST_CASE("the legacy solver still plays a frame")
{
    MatchSession session(eightBallVariant(), ShotInputTuning{}, PhysicsBackend::Legacy);
    chargeAndRelease(session, 1.0f, 60.0f);
    CHECK(session.activeTrajectory() == nullptr);
    stepUntilSettled(session);
    CHECK(session.resolvedShotCount() == 1);
}
