#include "gameplay/director.h"
#include "gameplay/match_session.h"
#include "scene/components.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace BilliardsSaloon;

namespace
{
    DirectorTable tableFor(const MatchSession& session)
    {
        DirectorTable table;
        table.length = session.variant().table.clothWidth;
        table.width = session.variant().table.clothDepth;
        table.ballRadius = session.variant().table.ballRadius;
        table.pockets = session.pocketPositions();
        return table;
    }

    void fire(MatchSession& session, float aimRadians, float power01)
    {
        ShotControls aim;
        aim.aimDeltaRadians = aimRadians - session.shotState().aimAngleRadians;
        session.applyShotControls(aim, 0.0f);
        ShotControls hold;
        hold.shootHeld = true;
        const int frames = std::max(1, static_cast<int>(std::lround(power01 / 0.9f * 120.0f)));
        for (int i = 0; i < frames; ++i)
        {
            session.applyShotControls(hold, 1.0f / 120.0f);
        }
        session.applyShotControls(ShotControls{}, 1.0f / 120.0f);
    }

    MatchSettings fixed()
    {
        MatchSettings settings;
        settings.shuffleRack = false;
        return settings;
    }
}

TEST_CASE("the director cuts to the pocket before the ball drops")
{
    MatchSession session(nineBallVariant(), {}, fixed());
    const glm::vec2 corner(0.5f * 2.54f, 0.5f * 1.27f);
    const glm::vec2 diagonal = glm::normalize(glm::vec2(1.0f, 1.0f));
    // A slow, long pot: the drop comes late enough to cut to the pocket.
    const glm::vec2 object = corner - diagonal * 1.0f;
    session.setLayout(object - diagonal * 0.3f, {{9, object}});
    fire(session, 0.75f * 3.14159265f, 0.08f);

    const Sim::ShotTrajectory* trajectory = session.activeTrajectory();
    REQUIRE(trajectory != nullptr);
    const std::vector<DirectorCut> cuts = planShotCoverage(*trajectory, tableFor(session));

    REQUIRE(cuts.size() >= 2);
    CHECK(cuts.front().shot == DirectorShot::Player);
    CHECK(cuts[1].shot == DirectorShot::Pocket);

    double drop = 0.0;
    for (const Sim::ShotEvent& event : trajectory->events)
    {
        if ((event.type == Sim::EventType::Pocket) && (event.ball != 0))
        {
            drop = event.time;
            break;
        }
    }
    REQUIRE(drop > 0.0);
    CHECK(cuts[1].time < drop);
    CHECK(cuts[1].time >= 0.35);

    // The camera sits outside the cloth, above the rail, looking at the table.
    CHECK(std::abs(cuts[1].position.x) + std::abs(cuts[1].position.z) > 1.4f);
    CHECK(cuts[1].position.y > 0.2f);
    CHECK(glm::length(glm::vec2(cuts[1].target.x, cuts[1].target.z)) <
          glm::length(glm::vec2(cuts[1].position.x, cuts[1].position.z)));

    CHECK(cutAt(cuts, 0.0).shot == DirectorShot::Player);
    CHECK(cutAt(cuts, drop).shot == DirectorShot::Pocket);
}

TEST_CASE("a long shot with nothing down goes wide; a short one stays with the player")
{
    MatchSession session(nineBallVariant(), {}, fixed());
    session.setLayout(glm::vec2(-0.6f, 0.0f), {{1, glm::vec2(0.9f, 0.5f)}});
    fire(session, -0.5f * 3.14159265f, 0.6f);   // away from the ball, off the head cushion
    REQUIRE(session.activeTrajectory() != nullptr);
    const std::vector<DirectorCut> lengthy = planShotCoverage(*session.activeTrajectory(), tableFor(session));
    CHECK(lengthy.back().shot == DirectorShot::Wide);

    MatchSession soft(nineBallVariant(), {}, fixed());
    soft.setLayout(glm::vec2(0.0f, 0.0f), {{1, glm::vec2(0.9f, 0.5f)}});
    fire(soft, 0.0f, 0.0f);
    REQUIRE(soft.activeTrajectory() != nullptr);
    if (soft.activeTrajectory()->duration() < 2.2)
    {
        CHECK(planShotCoverage(*soft.activeTrajectory(), tableFor(soft)).size() == 1);
    }
}

TEST_CASE("a replay plays the last shot again and puts the table back")
{
    MatchSession session(nineBallVariant(), {}, fixed());
    const glm::vec2 corner(0.5f * 2.54f, 0.5f * 1.27f);
    const glm::vec2 diagonal = glm::normalize(glm::vec2(1.0f, 1.0f));
    const glm::vec2 object = corner - diagonal * 0.2f;
    session.setLayout(object - diagonal * 0.3f, {{1, glm::vec2(-0.9f, -0.4f)}, {9, object}});
    CHECK_FALSE(session.canReplay());   // nothing played yet

    fire(session, 0.75f * 3.14159265f, 0.3f);
    for (int i = 0; (i < 120 * 30) && (session.shotState().phase == ShotPhase::BallsInMotion); ++i)
    {
        session.step(1.0 / 120.0);
    }
    // Hitting the 9 first is a foul: the 9 is spotted and player 2 has ball in hand.
    REQUIRE(session.placingCueBall());
    const glm::vec3 nineAfter = session.registry().get<TransformComponent>(*session.ballEntity(9)).position;
    const glm::vec3 cueAfter = session.registry().get<TransformComponent>(session.cueBallEntity()).position;

    REQUIRE(session.canReplay());
    REQUIRE(session.startReplay(0.5));
    CHECK(session.replaying());
    CHECK_FALSE(session.acceptsShotInput());
    CHECK_FALSE(session.registry().get<BallComponent>(*session.ballEntity(9)).pocketed);   // back at the start

    bool droppedDuringReplay = false;
    for (int i = 0; (i < 120 * 60) && session.replaying(); ++i)
    {
        session.step(1.0 / 120.0);
        droppedDuringReplay = droppedDuringReplay || session.registry().get<BallComponent>(*session.ballEntity(9)).pocketed;
    }
    CHECK(droppedDuringReplay);
    CHECK_FALSE(session.replaying());

    // Exactly as before: the spotted 9 and the cue ball in hand.
    CHECK_FALSE(session.registry().get<BallComponent>(*session.ballEntity(9)).pocketed);
    CHECK(glm::length(session.registry().get<TransformComponent>(*session.ballEntity(9)).position - nineAfter) < 1.0e-6f);
    CHECK(glm::length(session.registry().get<TransformComponent>(session.cueBallEntity()).position - cueAfter) < 1.0e-6f);
    CHECK(session.placingCueBall());
}
