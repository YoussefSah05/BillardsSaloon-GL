// Characterization tests for the prototype fixed-step solver. They pin the
// current behaviour (and its agreement with closed-form cloth physics) so the
// refactors before the event-based rewrite cannot change gameplay silently.

#include "physics/billiards_physics.h"
#include "scene/components.h"

#include <doctest/doctest.h>

#include <glm/glm.hpp>

using namespace BilliardsSaloon;

namespace
{
    constexpr double STEP = 1.0 / 120.0;
    constexpr float GRAVITY = 9.81f;
    constexpr float RADIUS = 0.028575f;

    Entity spawnBall(
        Registry& registry,
        const glm::vec3& position,
        const glm::vec3& linearVelocity,
        const glm::vec3& angularVelocity = glm::vec3(0.0f),
        int number = 0)
    {
        const Entity entity = registry.createEntity();

        TransformComponent transform;
        transform.position = position;
        transform.syncPrevious();
        registry.emplace<TransformComponent>(entity, transform);

        BallComponent ball;
        ball.linearVelocity = linearVelocity;
        ball.angularVelocity = angularVelocity;
        ball.number = number;
        ball.isCueBall = (number == 0);
        ball.ruleTag = (number == 0) ? BallRuleTag::Cue : BallRuleTag::Solid;
        registry.emplace<BallComponent>(entity, ball);

        return entity;
    }

    void addTable(Registry& registry)
    {
        registry.emplace<TableBoundsComponent>(registry.createEntity(), TableBoundsComponent{});
    }

    void simulate(Registry& registry, ShotResult& result, double seconds)
    {
        const int steps = static_cast<int>(seconds / STEP);
        for (int i = 0; i < steps; ++i)
        {
            Physics::stepBilliardsWorld(registry, STEP, result);
        }
    }

    float kineticEnergy(Registry& registry)
    {
        float energy = 0.0f;
        registry.view<BallComponent>().each(
            [&](Entity, BallComponent& ball)
            {
                const float inertia = 0.4f * ball.massKg * ball.radius * ball.radius;
                energy += 0.5f * ball.massKg * glm::dot(ball.linearVelocity, ball.linearVelocity);
                energy += 0.5f * inertia * glm::dot(ball.angularVelocity, ball.angularVelocity);
            }
        );
        return energy;
    }
}

TEST_CASE("a stun shot settles into rolling at 5/7 of its speed")
{
    Registry registry;
    ShotResult result;
    const Entity ball = spawnBall(registry, {0.0f, RADIUS, 0.0f}, {1.0f, 0.0f, 0.0f});

    simulate(registry, result, 0.5);

    // Sliding lasts 2 v0 / (7 mu_s g); afterwards rolling resistance applies.
    constexpr float muSlide = 0.20f;
    constexpr float muRoll = 0.010f;
    const float slideTime = 2.0f / (7.0f * muSlide * GRAVITY);
    const float expectedSpeed = (5.0f / 7.0f) - muRoll * GRAVITY * (0.5f - slideTime);

    const BallComponent& state = registry.get<BallComponent>(ball);
    CHECK(state.linearVelocity.x == doctest::Approx(expectedSpeed).epsilon(0.01));

    // Rolling without slipping: v_x = -R * w_z.
    CHECK(state.linearVelocity.x == doctest::Approx(-RADIUS * state.angularVelocity.z).epsilon(0.001));
}

TEST_CASE("a rolling ball stops after v^2 / (2 mu_r g)")
{
    Registry registry;
    ShotResult result;
    const Entity ball = spawnBall(
        registry,
        {0.0f, RADIUS, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -1.0f / RADIUS}
    );

    simulate(registry, result, 12.0);

    const float expectedDistance = 1.0f / (2.0f * 0.010f * GRAVITY);
    CHECK(registry.get<TransformComponent>(ball).position.x == doctest::Approx(expectedDistance).epsilon(0.01));
    CHECK_FALSE(Physics::anyBallInMotion(registry));
}

TEST_CASE("a head-on hit transfers most of the cue ball's speed and records first contact")
{
    Registry registry;
    ShotResult result;
    addTable(registry);
    const Entity cue = spawnBall(registry, {-0.3f, RADIUS, 0.0f}, {1.5f, 0.0f, 0.0f});
    const Entity object = spawnBall(registry, {0.0f, RADIUS, 0.0f}, {0.0f, 0.0f, 0.0f}, glm::vec3(0.0f), 3);

    for (int i = 0; (i < 240) && (result.firstObjectBallNumber < 0); ++i)
    {
        Physics::stepBilliardsWorld(registry, STEP, result);
    }

    REQUIRE(result.firstObjectBallNumber == 3);
    CHECK(result.firstObjectBallTag == BallRuleTag::Solid);

    const float cueSpeed = registry.get<BallComponent>(cue).linearVelocity.x;
    const float objectSpeed = registry.get<BallComponent>(object).linearVelocity.x;
    CHECK(objectSpeed > 0.8f);
    CHECK(cueSpeed < 0.2f * objectSpeed);
}

TEST_CASE("a ball driven into a cushion rebounds and stays on the table")
{
    Registry registry;
    ShotResult result;
    addTable(registry);
    const Entity ball = spawnBall(registry, {1.2f, RADIUS, 0.0f}, {1.0f, 0.0f, 0.0f});

    simulate(registry, result, 0.5);

    const TableBoundsComponent bounds;
    CHECK(registry.get<BallComponent>(ball).linearVelocity.x < 0.0f);
    CHECK(registry.get<TransformComponent>(ball).position.x <= bounds.halfWidth - RADIUS + 1.0e-5f);
    CHECK(result.pocketedBalls.empty());
}

TEST_CASE("a ball sent at a corner pocket is captured and reported")
{
    Registry registry;
    ShotResult result;
    addTable(registry);
    const glm::vec3 toCorner = glm::normalize(glm::vec3(0.22f, 0.0f, 0.21f));
    const Entity ball = spawnBall(registry, {1.2f, RADIUS, 0.5f}, 1.5f * toCorner, glm::vec3(0.0f), 5);

    simulate(registry, result, 1.0);

    CHECK(registry.get<BallComponent>(ball).pocketed);
    REQUIRE(result.pocketedBalls.size() == 1);
    CHECK(result.pocketedBalls.front().number == 5);
    CHECK_FALSE(result.cueBallPocketed);
}

TEST_CASE("kinetic energy never increases during a shot")
{
    Registry registry;
    ShotResult result;
    addTable(registry);
    spawnBall(registry, {-0.5f, RADIUS, 0.05f}, {3.0f, 0.0f, 0.4f}, {0.0f, 40.0f, 0.0f});
    spawnBall(registry, {0.2f, RADIUS, 0.0f}, glm::vec3(0.0f), glm::vec3(0.0f), 1);
    spawnBall(registry, {0.26f, RADIUS, 0.03f}, glm::vec3(0.0f), glm::vec3(0.0f), 2);

    float previous = kineticEnergy(registry);
    for (int i = 0; i < 600; ++i)
    {
        Physics::stepBilliardsWorld(registry, STEP, result);
        const float current = kineticEnergy(registry);
        CHECK(current <= previous * 1.0001f + 1.0e-6f);
        previous = current;
    }
}

TEST_CASE("anyBallInMotion reflects ball state")
{
    Registry registry;
    const Entity ball = spawnBall(registry, {0.0f, RADIUS, 0.0f}, glm::vec3(0.0f));
    CHECK_FALSE(Physics::anyBallInMotion(registry));

    registry.get<BallComponent>(ball).linearVelocity.x = 0.5f;
    CHECK(Physics::anyBallInMotion(registry));
}
