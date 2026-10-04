#include "gameplay/game_variant.h"

#include <doctest/doctest.h>

#include <glm/glm.hpp>

#include <vector>

using namespace BilliardsSaloon;

namespace
{
    void checkRackIsValid(const GameVariantDefinition& variant)
    {
        const std::vector<glm::vec3> positions = buildRackPositions(variant);
        REQUIRE(positions.size() == variant.objectBalls.size());

        const float radius = variant.table.ballRadius;
        const float halfWidth = 0.5f * variant.table.clothWidth;
        const float halfDepth = 0.5f * variant.table.clothDepth;

        for (std::size_t i = 0; i < positions.size(); ++i)
        {
            CHECK(std::abs(positions[i].x) <= halfWidth - radius);
            CHECK(std::abs(positions[i].z) <= halfDepth - radius);
            CHECK(positions[i].y == doctest::Approx(radius));

            for (std::size_t j = i + 1; j < positions.size(); ++j)
            {
                // Tangent balls are allowed; overlapping ones are not.
                CHECK(glm::distance(positions[i], positions[j]) >= 2.0f * radius - 1.0e-5f);
            }
        }
    }
}

TEST_CASE("8-ball racks fifteen non-overlapping balls on the table")
{
    CHECK(eightBallVariant().objectBalls.size() == 15);
    checkRackIsValid(eightBallVariant());
}

TEST_CASE("9-ball racks nine non-overlapping balls in a diamond")
{
    CHECK(nineBallVariant().objectBalls.size() == 9);
    checkRackIsValid(nineBallVariant());
}
