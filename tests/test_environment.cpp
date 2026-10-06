#include "ai/environment.h"
#include "ai/planner.h"

#include <doctest/doctest.h>

#include <chrono>

using namespace BilliardsSaloon;

TEST_CASE("environment: frames play out instantly between classical players")
{
    Ai::Environment env(GameDiscipline::NineBall, 3);
    CHECK(env.decision() == Ai::Decision::Break);
    CHECK(env.toAct() == 0);

    Ai::AiProfile profile = Ai::findAiProfile("dani_reyes");
    profile.samples = 2;

    const auto start = std::chrono::steady_clock::now();
    int frames = 0;
    for (std::uint32_t game = 0; game < 2; ++game)
    {
        env.reset(100 + game, static_cast<int>(game % 2));
        for (int move = 0; (move < 200) && (env.decision() != Ai::Decision::FrameOver); ++move)
        {
            (void)env.playClassical(profile, 1000 * game + static_cast<std::uint32_t>(move));
        }
        CHECK(env.decision() == Ai::Decision::FrameOver);
        CHECK(env.frame().winner >= 0);
        frames += (env.decision() == Ai::Decision::FrameOver) ? 1 : 0;
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    MESSAGE("2 classical 9-ball frames in " << seconds << " s");
    CHECK(frames == 2);
}

TEST_CASE("environment: a shot is simulated, judged and applied in one call")
{
    Ai::Environment env(GameDiscipline::EightBall, 9);
    Ai::AiShot plan = Ai::planBreak(env.observe(), Ai::findAiProfile("marta_okafor"));
    const Ai::StepResult result = env.shoot(plan.input);
    CHECK(result.shooter == 0);
    CHECK(result.verdict.breakShot);
    CHECK(env.shotsPlayed() == 1);
    CHECK(env.decision() != Ai::Decision::Break);
}

#include "ai/features.h"

TEST_CASE("features: fixed size, perspective-dependent, in range")
{
    Ai::Environment env(GameDiscipline::EightBall, 5);
    const Ai::AiTable table = env.observe();
    const std::vector<float> me = Ai::encodeTable(table, 0);
    const std::vector<float> them = Ai::encodeTable(table, 1);
    REQUIRE(me.size() == Ai::FEATURE_COUNT);
    CHECK(me != them);   // "my turn" differs
    for (const float f : me)
    {
        CHECK(f >= -1.01f);
        CHECK(f <= 1.01f);
    }
    // All 16 balls present at the start.
    for (std::size_t slot = 0; slot < Ai::BALL_SLOTS; ++slot)
    {
        CHECK(me[slot * Ai::BALL_FEATURES] == 1.0f);
    }
}
