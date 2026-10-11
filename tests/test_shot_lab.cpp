#include "rl/geometry.h"
#include "rl/layouts.h"
#include "rl/shot_lab.h"

#include <doctest/doctest.h>

#include <cmath>
#include <cstdio>
#include <random>

using namespace Septhuit;
using namespace Septhuit::Rl;

namespace
{
    const Sim::Table& table()
    {
        static const Sim::Table t = Sim::buildPocketTable({});
        return t;
    }

    const Sim::BallParams PARAMS;
}

TEST_CASE("pocket mouths sit between each pocket's jaws and open towards it")
{
    const std::vector<PocketMouth> mouths = pocketMouths(table());
    REQUIRE(mouths.size() == 6);
    for (const PocketMouth& mouth : mouths)
    {
        CAPTURE(mouth.pocket);
        const Sim::Pocket& pocket = table().pockets[static_cast<std::size_t>(mouth.pocket)];
        const glm::dvec2 toPocket = glm::dvec2(pocket.center.x, pocket.center.y) - mouth.centre;
        // The pocket lies beyond the mouth, a few centimetres away.
        CHECK(glm::length(toPocket) > 0.005);
        CHECK(glm::length(toPocket) < 0.12);
        CHECK(glm::dot(glm::normalize(toPocket), mouth.inward) == doctest::Approx(1.0));
        // A ball fits through: the gap exceeds a ball's diameter.
        CHECK(2.0 * mouth.halfWidth > 2.0 * PARAMS.R);
        CHECK(2.0 * mouth.halfWidth < 0.2);
    }
}

TEST_CASE("ghost-ball geometry: straight and cut shots")
{
    const glm::dvec2 target(0.5, 2.0);
    const glm::dvec2 object(0.5, 1.5);
    const GhostShot straight = ghostShot({0.5, 1.0}, object, target, PARAMS.R);
    CHECK(straight.cutDegrees == doctest::Approx(0.0).epsilon(1e-9));
    CHECK(straight.phiDegrees == doctest::Approx(90.0));
    CHECK(straight.ghost.y == doctest::Approx(1.5 - 2.0 * PARAMS.R));
    CHECK(straight.objectDistance == doctest::Approx(0.5));

    // Approaching from the side: the cut grows with the angle between paths.
    const GhostShot cut = ghostShot({0.1, 1.5 - 2.0 * PARAMS.R - 0.3}, object, target, PARAMS.R);
    CHECK(cut.cutDegrees > 30.0);
    CHECK(cut.cutDegrees < 70.0);
}

TEST_CASE("the speed model asks for more pace on longer and thinner shots")
{
    GhostShot shot;
    shot.cueDistance = 0.5;
    shot.objectDistance = 0.5;
    const double base = cueSpeedFor(shot, PARAMS, {});
    shot.objectDistance = 1.5;
    const double longer = cueSpeedFor(shot, PARAMS, {});
    shot.cutDegrees = 50.0;
    const double thinner = cueSpeedFor(shot, PARAMS, {});
    CHECK(base > 0.3);
    CHECK(longer > base);
    CHECK(thinner > longer);
}

TEST_CASE("outcomes read pots, scratches and first contact from the events")
{
    const std::vector<PocketMouth> mouths = pocketMouths(table());

    // Cue ball rolled straight at a corner mouth with nothing in the way: a scratch.
    const glm::dvec2 cue(0.6, 0.6);
    const PocketMouth& corner = mouths[0];
    const glm::dvec2 aim = corner.centre - cue;
    Sim::CueStrike strike;
    strike.speed = 2.0;
    strike.phiDegrees = std::atan2(aim.y, aim.x) * 180.0 / 3.14159265358979323846;
    const ShotOutcome scratch = simulateOutcome(table(), {cue, {1.0, 2.2}}, strike);
    CHECK(scratch.scratch);
    CHECK(scratch.pocketOf[0] == corner.pocket);
    CHECK(scratch.firstContact == -1);
    CHECK(scratch.complete);

    // A straight-in pot: the object ball 30 cm from the mouth, cue ball behind it.
    const glm::dvec2 object = corner.centre - corner.inward * 0.3;
    const glm::dvec2 cueBehind = object - corner.inward * 0.4;
    const std::vector<glm::dvec2> layout {cueBehind, object};
    const std::optional<GeometryShot> shot = geometryShot(table(), mouths, layout, 1, PARAMS, 75.0, corner.pocket);
    REQUIRE(shot.has_value());
    const ShotOutcome pot = simulateOutcome(table(), layout, shot->strike);
    CHECK(pot.potted(1));
    CHECK(pot.pocketOf[1] == corner.pocket);
    CHECK_FALSE(pot.scratch);
    CHECK(pot.firstContact == 1);
    CHECK(pot.potOrder == std::vector<int>{1});
    CHECK(pot.cushionsBeforePot[1] == 0);
    // Straight in: the object ball leaves along the line to the mouth.
    REQUIRE(glm::length(pot.contactVelocity) > 0.1);
    CHECK(glm::dot(glm::normalize(pot.contactVelocity), glm::normalize(corner.centre - object)) > 0.999);
}

TEST_CASE("a batch gives exactly the shots played one by one")
{
    LayoutGenerator generator(table(), 11);
    std::vector<std::vector<glm::dvec2>> layouts;
    std::vector<Sim::CueStrike> strikes;
    for (int i = 0; i < 64; ++i)
    {
        layouts.push_back(generator.singleBall(Difficulty::Any).positions);
        Sim::CueStrike strike;
        strike.speed = 0.5 + 0.1 * i;
        strike.phiDegrees = 5.6 * i;
        strikes.push_back(strike);
    }
    const std::vector<ShotOutcome> batch = simulateBatch(table(), layouts, strikes, PARAMS, 4);
    REQUIRE(batch.size() == layouts.size());
    for (std::size_t i = 0; i < layouts.size(); ++i)
    {
        const ShotOutcome one = simulateOutcome(table(), layouts[i], strikes[i]);
        CHECK(one.pocketOf == batch[i].pocketOf);
        CHECK(one.events == batch[i].events);
        CHECK(one.finalPositions[0].x == batch[i].finalPositions[0].x);   // bitwise
        CHECK(one.finalPositions[1].y == batch[i].finalPositions[1].y);
    }
}

TEST_CASE("the layout generator is deterministic, legal and honours its bands")
{
    LayoutGenerator a(table(), 2026);
    LayoutGenerator b(table(), 2026);
    for (const Difficulty band : {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard, Difficulty::Any})
    {
        CAPTURE(difficultyName(band));
        for (int i = 0; i < 50; ++i)
        {
            const SingleBallLayout la = a.singleBall(band);
            const SingleBallLayout lb = b.singleBall(band);
            REQUIRE(la.positions.size() == 2);
            CHECK(la.positions[0] == lb.positions[0]);
            CHECK(la.positions[1] == lb.positions[1]);
            CHECK(a.legalSpot(la.positions[0], {}));
            CHECK(a.legalSpot(la.positions[1], {la.positions[0]}));
            if (band != Difficulty::Any)
            {
                CHECK(la.difficulty == band);
                CHECK(la.pocket >= 0);
                CHECK(classifyShot(la.ghost) == band);
            }
        }
    }
    CHECK(difficultyFromName("medium") == Difficulty::Medium);
    CHECK_THROWS((void)difficultyFromName("impossible"));
}

TEST_CASE("the geometry baseline is strong without noise; difficulty shows under aiming error")
{
    // With perfect execution, aiming at the mouth pots most layouts in every
    // band (misses come from throw and the jaws). Bands measure how much an
    // aiming error is amplified (docs/design/AI.md), so they separate only
    // once the aim is perturbed.
    const std::vector<PocketMouth> mouths = pocketMouths(table());
    const Difficulty bands[3] = {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard};
    constexpr int N = 300;
    constexpr double SIGMA_DEGREES = 0.25;
    double clean[3] = {};
    double noisy[3] = {};
    std::mt19937_64 noise(99);
    std::normal_distribution<double> gauss(0.0, SIGMA_DEGREES);
    for (int b = 0; b < 3; ++b)
    {
        LayoutGenerator generator(table(), 7);
        int potted = 0;
        int pottedNoisy = 0;
        for (int i = 0; i < N; ++i)
        {
            const SingleBallLayout layout = generator.singleBall(bands[b]);
            const std::optional<GeometryShot> shot = geometryShot(table(), mouths, layout.positions, 1, PARAMS);
            REQUIRE(shot.has_value());
            const ShotOutcome outcome = simulateOutcome(table(), layout.positions, shot->strike);
            potted += (outcome.potted(1) && !outcome.scratch) ? 1 : 0;
            Sim::CueStrike perturbed = shot->strike;
            perturbed.phiDegrees += gauss(noise);
            const ShotOutcome noisyOutcome = simulateOutcome(table(), layout.positions, perturbed);
            pottedNoisy += (noisyOutcome.potted(1) && !noisyOutcome.scratch) ? 1 : 0;
        }
        clean[b] = static_cast<double>(potted) / N;
        noisy[b] = static_cast<double>(pottedNoisy) / N;
    }
    std::printf("geometry baseline, no noise:      easy %.2f, medium %.2f, hard %.2f\n", clean[0], clean[1], clean[2]);
    std::printf("geometry baseline, aim sd %.2f deg: easy %.2f, medium %.2f, hard %.2f\n", SIGMA_DEGREES, noisy[0], noisy[1], noisy[2]);
    for (int b = 0; b < 3; ++b)
    {
        CHECK(clean[b] > 0.7);
    }
    CHECK(noisy[0] > noisy[1]);
    CHECK(noisy[1] > noisy[2]);
}
