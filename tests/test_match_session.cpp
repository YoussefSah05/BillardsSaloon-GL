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
    CHECK(session.matchState().flowPhase == MatchFlowPhase::BreakShot);
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

    ShotControls controls;
    controls.aimAxis = 1.0f;
    controls.strikeRightAxis = 1.0f;
    controls.strikeForwardAxis = 1.0f;
    for (int i = 0; i < 120; ++i)
    {
        session.applyShotControls(controls, 1.0f / 60.0f);
    }

    const ShotInputTuning tuning;
    CHECK(session.shotState().aimAngleRadians == doctest::Approx(2.0f * tuning.aimRadiansPerSecond));

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
    CHECK(session.matchState().flowPhase != MatchFlowPhase::BreakShot);

    if (session.matchState().flowPhase != MatchFlowPhase::FrameOver)
    {
        CHECK(session.shotState().phase == ShotPhase::Aiming);
        CHECK(session.acceptsShotInput());

        const BallComponent& cueBall = session.registry().get<BallComponent>(session.cueBallEntity());
        CHECK_FALSE(cueBall.pocketed);
    }
}

TEST_CASE("a button held through a pause does not fire on release")
{
    MatchSession session(eightBallVariant());
    session.applyShotControls(holdShoot(), 1.0f / 60.0f);
    session.cancelHeldShot();

    // Simulates resuming with the button already released: the charge phase
    // only fires on a release it saw held, so nothing happens yet.
    session.applyShotControls(ShotControls{}, 1.0f / 60.0f);
    CHECK(session.shotState().phase == ShotPhase::Charging);
    CHECK_FALSE(session.ballsInMotion());
}

TEST_CASE("resetting the rack restores the opening position")
{
    MatchSession session(eightBallVariant());
    chargeAndRelease(session, 1.0f, 60.0f);
    stepUntilSettled(session);

    session.resetRack();

    CHECK(session.matchState().flowPhase == MatchFlowPhase::BreakShot);
    CHECK(session.matchState().activePlayerIndex == 0);
    for (const Entity ball : session.objectBallEntities())
    {
        CHECK_FALSE(session.registry().get<BallComponent>(ball).pocketed);
    }
}
