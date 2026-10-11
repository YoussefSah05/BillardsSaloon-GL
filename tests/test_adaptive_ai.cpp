#include "ai/opponent_model.h"
#include "ai/features.h"
#include "ai/environment.h"
#include "gameplay/match_session.h"
#include <doctest/doctest.h>
#include <algorithm>
#include <cmath>

using namespace Septhuit;

TEST_CASE("adaptive skill ignores breaks and safeties, changes gradually on evidence")
{
    Ai::OpponentModel strong;
    const auto adaptive = Ai::findAiProfile("adaptive");
    CHECK(adaptive.adaptive);
    strong.observe(.8f, true, false, true);
    strong.observe(0.0f, false, false, false);
    CHECK(strong.observations() == 0);
    const float initial = strong.skill();
    strong.observe(.4f, true, false, false);
    CHECK(strong.skill() > initial);
    CHECK(strong.skill() - initial < .02f);
    Ai::OpponentModel weak;
    for (int i = 0; i < 60; ++i)
    {
        strong.observe(.4f, true, false, false);
        weak.observe(.8f, false, i % 3 == 0, false);
    }
    CHECK(strong.skill() > .9f);
    CHECK(weak.skill() < .2f);
    CHECK(strong.adapt(adaptive).aimNoiseDegrees < weak.adapt(adaptive).aimNoiseDegrees);
    CHECK(strong.adapt(adaptive).safetyBias == adaptive.safetyBias);
    const auto fixed = Ai::findAiProfile("sam_whitlock");
    CHECK(strong.adapt(fixed).aimNoiseDegrees == fixed.aimNoiseDegrees);
    strong.reset();
    CHECK(strong.observations() == 0);
    CHECK(strong.skill() == .5f);
}

TEST_CASE("adaptive encoder preserves ball identity under storage permutations and conditions on opponent")
{
    Ai::Environment env(GameDiscipline::NineBall, 1);
    Ai::AiTable table = env.observe();
    const auto own = Ai::findAiProfile("iris_lindqvist");
    const auto weak = Ai::findAiProfile("sam_whitlock");
    const auto strong = Ai::findAiProfile("viktor_hale");
    const auto features = Ai::encodeAdaptiveTable(table, 0, own, weak);
    REQUIRE(features.size() == 256);
    for (const float value : features) CHECK(std::isfinite(value));
    const auto stronger = Ai::encodeAdaptiveTable(table, 0, own, strong);
    CHECK(std::equal(features.begin(), features.end()-8, stronger.begin()));
    CHECK(features != stronger);
    std::swap(table.numbers[1], table.numbers[2]);
    std::swap(table.positions[1], table.positions[2]);
    CHECK(Ai::encodeAdaptiveTable(table, 0, own, weak) == features);
    for (std::size_t i = 1; i < table.pocketed.size(); ++i) table.pocketed[i] = true;
    const auto empty = Ai::encodeAdaptiveTable(table, 0, own, weak);
    for (std::size_t i = Ai::ADAPTIVE_BALL_FEATURES; i < 16*Ai::ADAPTIVE_BALL_FEATURES; ++i)
        CHECK(empty[i] == 0.0f);
}

TEST_CASE("pot evidence recognises an aimed shot and excludes a deliberate safety")
{
    MatchSession session(nineBallVariant());
    const glm::vec2 corner(1.27f, .635f);
    const glm::vec2 direction = glm::normalize(glm::vec2(1.f, 1.f));
    session.setLayout(corner-direction*.75f, {{9, corner-direction*.35f}});
    Ai::AiTable table = session.aiView();
    table.frame.phase = Rules::Phase::Play;
    const auto plan = Ai::planShot(table, Ai::findAiProfile("viktor_hale"), 1);
    CHECK(Ai::aimedPotEase(table, plan.input.aimRadians) > .1f);
    CHECK(Ai::aimedPotEase(table, plan.input.aimRadians+1.f) == 0.f);
    table.frame.phase = Rules::Phase::Break;
    CHECK(Ai::aimedPotEase(table, plan.input.aimRadians) == 0.f);
}
