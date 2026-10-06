// The classical AI on the real simulator and referee.

#include "ai/planner.h"
#include "gameplay/match_session.h"
#include "rules/shot_record.h"
#include "scene/components.h"

#include <doctest/doctest.h>

#include <cmath>
#include <initializer_list>
#include <random>

using namespace BilliardsSaloon;

namespace
{
    constexpr double STEP = 1.0 / 120.0;

    MatchSettings fixedRack()
    {
        MatchSettings settings;
        settings.shuffleRack = false;
        return settings;
    }

    void settle(MatchSession& session)
    {
        for (int i = 0; (i < 120 * 60) && (session.shotState().phase == ShotPhase::BallsInMotion); ++i)
        {
            session.step(STEP);
        }
    }

    // Pretend the break is done: rotation games in play, 8-ball on an open table.
    void skipBreak(MatchSession& session, std::vector<std::pair<int, glm::vec2>> balls, glm::vec2 cue)
    {
        session.setLayout(cue, balls);
    }

    Ai::AiTable inPlay(const MatchSession& session)
    {
        Ai::AiTable table = session.aiView();
        table.frame.phase = (table.discipline == GameDiscipline::EightBall) ? Rules::Phase::Open : Rules::Phase::Play;
        table.frame.ballInHand = Rules::BallInHand::None;
        return table;
    }

    Ai::AiProfile perfect(const std::string& id)
    {
        Ai::AiProfile p = Ai::findAiProfile(id);
        p.aimNoiseDegrees = 0.0f;
        p.powerNoise = 0.0f;
        p.spinNoise = 0.0f;
        p.samples = 1;
        return p;
    }

    // Plays the plan on the simulator and says what happened.
    Rules::ShotRecord simulate(const Ai::AiTable& table, const ShotInput& input)
    {
        std::vector<Sim::BallState> balls;
        for (std::size_t i = 0; i < table.positions.size(); ++i)
        {
            Sim::BallState s;
            s.r = SimBridge::toSimPosition(glm::vec3(table.positions[i].x, table.radius, table.positions[i].y), table.length, table.width);
            s.r.z = table.ball.R;
            s.s = table.pocketed[i] ? Sim::MotionState::Pocketed : Sim::MotionState::Stationary;
            balls.push_back(s);
        }
        const Sim::ShotTrajectory shot = Sim::simulateShot(table.table, balls, 0, toCueStrike(table.tuning, input), table.ball);
        return Rules::recordShot(shot, table.numbers);
    }

    std::vector<std::pair<int, glm::vec2>> randomLayout(std::mt19937& random, const std::vector<int>& numbers, float r)
    {
        std::uniform_real_distribution<float> x(-1.15f, 1.15f);
        std::uniform_real_distribution<float> z(-0.53f, 0.53f);
        std::vector<std::pair<int, glm::vec2>> balls;
        for (const int number : numbers)
        {
            for (int attempt = 0; attempt < 100; ++attempt)
            {
                const glm::vec2 p(x(random), z(random));
                bool free = glm::length(p) > 0.1f;
                for (const auto& [n, q] : balls)
                {
                    free = free && (glm::length(p - q) > 4.0f * r);
                }
                if (free)
                {
                    balls.emplace_back(number, p);
                    break;
                }
            }
        }
        return balls;
    }
}

TEST_CASE("AI profiles load, ordered from club to champion")
{
    const std::vector<Ai::AiProfile>& profiles = Ai::aiProfiles();
    REQUIRE(profiles.size() >= 4);
    CHECK(Ai::findAiProfile("viktor_hale").aimNoiseDegrees < Ai::findAiProfile("sam_whitlock").aimNoiseDegrees);
    CHECK(Ai::findAiProfile("nobody").id == profiles.front().id);
}

TEST_CASE("AI pots an easy ball")
{
    MatchSession session(nineBallVariant(), {}, fixedRack());
    // The 9 in front of a corner pocket, the cue ball straight behind it.
    const glm::vec2 corner(1.27f, 0.635f);
    const glm::vec2 dir = glm::normalize(glm::vec2(1.0f, 1.0f));
    skipBreak(session, {{9, corner - dir * 0.35f}}, corner - dir * 0.75f);
    const Ai::AiTable table = inPlay(session);

    const Ai::AiShot shot = Ai::planShot(table, perfect("viktor_hale"), 1);
    CAPTURE(shot.description);
    CHECK_FALSE(shot.safety);
    CHECK(shot.score > 50.0);
    const Rules::ShotRecord record = simulate(table, shot.input);
    CHECK(record.firstContact == 9);
    CHECK(record.potted(9));
    CHECK_FALSE(record.potted(Rules::CUE_BALL));
}

TEST_CASE("AI hits a legal ball first on random tables")
{
    std::mt19937 random(2026);
    MatchSession session(eightBallVariant(), {}, fixedRack());
    const float r = session.variant().table.ballRadius;
    int legal = 0;
    constexpr int TABLES = 12;
    for (int t = 0; t < TABLES; ++t)
    {
        skipBreak(session, randomLayout(random, {1, 3, 5, 8, 10, 12, 14}, r), glm::vec2(-0.6f, 0.0f));
        Ai::AiTable table = inPlay(session);
        table.frame.phase = Rules::Phase::Groups;
        table.frame.groups = {Rules::Group::Solids, Rules::Group::Stripes};

        const Ai::AiShot shot = Ai::planShot(table, perfect("dani_reyes"), static_cast<std::uint32_t>(t));
        const Rules::ShotRecord record = simulate(table, shot.input);
        legal += ((record.firstContact >= 1) && (record.firstContact <= 7)) ? 1 : 0;
    }
    CHECK(legal >= TABLES - 1);
}

TEST_CASE("a champion pots more than a club player")
{
    std::mt19937 random(7);
    MatchSession session(nineBallVariant(), {}, fixedRack());
    const float r = session.variant().table.ballRadius;
    int champion = 0;
    int club = 0;
    std::mt19937 execution(99);
    constexpr int TABLES = 14;
    for (int t = 0; t < TABLES; ++t)
    {
        skipBreak(session, randomLayout(random, {1, 9}, r), glm::vec2(-0.7f, 0.1f));
        const Ai::AiTable table = inPlay(session);
        for (const char* id : {"viktor_hale", "sam_whitlock"})
        {
            Ai::AiProfile profile = Ai::findAiProfile(id);
            profile.samples = 3;
            const Ai::AiShot shot = Ai::planShot(table, profile, static_cast<std::uint32_t>(t));
            const Rules::ShotRecord record = simulate(table, Ai::withExecutionError(shot.input, profile, execution));
            const bool potted = !record.objectPots().empty() && !record.potted(Rules::CUE_BALL) && (record.firstContact == 1);
            (std::string(id) == "viktor_hale" ? champion : club) += potted ? 1 : 0;
        }
    }
    CAPTURE(champion);
    CAPTURE(club);
    CHECK(champion > club);
}

TEST_CASE("the AI breaks into the rack and places the cue ball legally")
{
    MatchSession session(nineBallVariant(), {}, fixedRack());
    const Ai::AiTable table = session.aiView();
    const Ai::AiShot breakShot = Ai::planBreak(table, Ai::findAiProfile("marta_okafor"));
    const Rules::ShotRecord record = simulate(table, breakShot.input);
    CHECK(record.firstContact == 1);
    CHECK(record.objectBallsToRail >= 4);

    // Ball in hand anywhere after a foul: a legal spot, then a sensible shot.
    Ai::AiTable hand = inPlay(session);
    hand.frame.ballInHand = Rules::BallInHand::Anywhere;
    const glm::vec2 spot = Ai::planCueBallPlacement(hand, Ai::findAiProfile("dani_reyes"), 3);
    CHECK(std::abs(spot.x) < 1.27f);
    CHECK(std::abs(spot.y) < 0.635f);
    for (std::size_t i = 1; i < hand.positions.size(); ++i)
    {
        CHECK(glm::length(hand.positions[i] - spot) >= 2.0f * hand.radius);
    }
}

TEST_CASE("AI against AI: 9-ball frames finish")
{
    MatchSettings settings;
    settings.seed = 4;
    MatchSession session(nineBallVariant(), {}, settings);
    Ai::AiProfile profile = Ai::findAiProfile("dani_reyes");
    profile.samples = 2;
    std::mt19937 execution(5);

    int shots = 0;
    for (; (shots < 90) && !session.frameOver(); ++shots)
    {
        if (session.pendingChoice() != Rules::Choice::None)
        {
            REQUIRE(session.choose(Ai::planChoice(session.aiView(), profile, static_cast<std::uint32_t>(shots))));
            continue;
        }
        if (session.placingCueBall() || (session.canPlaceCueBall() && session.frame().phase != Rules::Phase::Break))
        {
            const glm::vec2 spot = Ai::planCueBallPlacement(session.aiView(), profile, static_cast<std::uint32_t>(shots));
            REQUIRE(session.placeCueBallAt(spot));
        }
        const Ai::AiTable table = session.aiView();
        const Ai::AiShot plan = (table.frame.phase == Rules::Phase::Break)
            ? Ai::planBreak(table, profile)
            : Ai::planShot(table, profile, static_cast<std::uint32_t>(shots));
        REQUIRE(session.playShot(Ai::withExecutionError(plan.input, profile, execution)));
        settle(session);
    }
    CAPTURE(shots);
    CHECK(session.frameOver());
}
