#include "ecs/registry.h"

#include <doctest/doctest.h>

#include <vector>

using namespace BilliardsSaloon;

namespace
{
    struct Position
    {
        float x {0.0f};
    };

    struct Velocity
    {
        float dx {0.0f};
    };
}

TEST_CASE("entities are alive until destroyed")
{
    Registry registry;
    const Entity entity = registry.createEntity();

    CHECK(registry.isAlive(entity));
    registry.destroyEntity(entity);
    CHECK_FALSE(registry.isAlive(entity));
}

TEST_CASE("recycled indices get a new generation so stale handles stay dead")
{
    Registry registry;
    const Entity first = registry.createEntity();
    registry.destroyEntity(first);

    const Entity second = registry.createEntity();
    CHECK(second.index == first.index);
    CHECK(second.generation != first.generation);
    CHECK_FALSE(registry.isAlive(first));
    CHECK(registry.isAlive(second));
}

TEST_CASE("components can be added, read and removed with their entity")
{
    Registry registry;
    const Entity entity = registry.createEntity();
    registry.emplace<Position>(entity, Position{3.0f});

    REQUIRE(registry.has<Position>(entity));
    CHECK(registry.get<Position>(entity).x == doctest::Approx(3.0f));
    CHECK(registry.tryGet<Velocity>(entity) == nullptr);

    registry.destroyEntity(entity);
    CHECK_FALSE(registry.has<Position>(entity));
}

TEST_CASE("view visits only entities that have every requested component")
{
    Registry registry;
    const Entity both = registry.createEntity();
    const Entity onlyPosition = registry.createEntity();

    registry.emplace<Position>(both, Position{1.0f});
    registry.emplace<Velocity>(both, Velocity{2.0f});
    registry.emplace<Position>(onlyPosition, Position{5.0f});

    std::vector<Entity> visited;
    registry.view<Position, Velocity>().each(
        [&](Entity entity, Position& position, Velocity& velocity)
        {
            position.x += velocity.dx;
            visited.push_back(entity);
        }
    );

    REQUIRE(visited.size() == 1);
    CHECK(visited.front() == both);
    CHECK(registry.get<Position>(both).x == doctest::Approx(3.0f));
    CHECK(registry.get<Position>(onlyPosition).x == doctest::Approx(5.0f));
}
