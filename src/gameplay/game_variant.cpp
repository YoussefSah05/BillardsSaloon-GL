#include "gameplay/game_variant.h"

#include <cmath>

namespace BilliardsSaloon
{
    namespace
    {
        BallSpawnDefinition makeBall(
            const std::string& name,
            int number,
            BallRuleTag ruleTag,
            const glm::vec3& albedo,
            bool isCueBall = false)
        {
            BallSpawnDefinition ball;
            ball.name = name;
            ball.number = number;
            ball.ruleTag = ruleTag;
            ball.albedo = albedo;
            ball.specularStrength = 0.92f;
            ball.shininess = 128.0f;
            ball.isCueBall = isCueBall;
            return ball;
        }

        std::vector<glm::vec3> buildTriangleRackPositions(
            std::size_t ballCount,
            float ballRadius,
            const glm::vec3& apexPosition,
            float spacingScale)
        {
            std::vector<glm::vec3> positions;
            positions.reserve(ballCount);

            const float diameter = 2.0f * ballRadius;
            const float rowSpacing = std::sqrt(3.0f) * ballRadius * spacingScale;

            std::size_t placed = 0;
            for (int row = 0; placed < ballCount; ++row)
            {
                const int rowCount = row + 1;
                const float z = apexPosition.z - static_cast<float>(row) * rowSpacing;

                for (int col = 0; (col < rowCount) && (placed < ballCount); ++col)
                {
                    const float x =
                        apexPosition.x +
                        (static_cast<float>(col) - 0.5f * static_cast<float>(row)) * diameter * spacingScale;

                    positions.push_back(glm::vec3(x, apexPosition.y, z));
                    ++placed;
                }
            }

            return positions;
        }

        std::vector<glm::vec3> buildDiamondRackPositions(
            std::size_t ballCount,
            float ballRadius,
            const glm::vec3& apexPosition,
            float spacingScale)
        {
            std::vector<glm::vec3> positions;
            positions.reserve(ballCount);

            const float diameter = 2.0f * ballRadius;
            const float rowSpacing = std::sqrt(3.0f) * ballRadius * spacingScale;

            const int rowCounts[5] = {1, 2, 3, 2, 1};

            std::size_t placed = 0;
            for (int row = 0; (row < 5) && (placed < ballCount); ++row)
            {
                const int rowCount = rowCounts[row];
                const float z = apexPosition.z - static_cast<float>(row) * rowSpacing;

                for (int col = 0; (col < rowCount) && (placed < ballCount); ++col)
                {
                    const float x =
                        apexPosition.x +
                        (static_cast<float>(col) - 0.5f * static_cast<float>(rowCount - 1)) * diameter * spacingScale;

                    positions.push_back(glm::vec3(x, apexPosition.y, z));
                    ++placed;
                }
            }

            return positions;
        }
    }

    const GameVariantDefinition& eightBallVariant()
    {
        static const GameVariantDefinition variant = []()
        {
            GameVariantDefinition v;
            v.discipline = GameDiscipline::EightBall;
            v.displayName = "8-Ball";

            v.table = TableSpecification{
                .clothWidth = 2.84f,
                .clothDepth = 1.42f,
                .ballRadius = 0.028575f,
                .ballMassKg = 0.17f
            };

            v.cueBall = BallSpawnDefinition{
                .name = "Cue Ball",
                .number = 0,
                .ruleTag = BallRuleTag::Cue,
                .albedo = glm::vec3(0.93f, 0.93f, 0.91f),
                .specularStrength = 0.95f,
                .shininess = 128.0f,
                .isCueBall = true
            };

            v.rack = RackSpecification{
                .pattern = RackPattern::Triangle,
                .apexPosition = glm::vec3(0.0f, v.table.ballRadius, -0.18f),
                .spacingScale = 1.0f
            };

            // Ordered in rack-fill order.
            // Row fill order for triangle:
            // 1, 2 3, 4 5 6, 7 8 9 10, 11 12 13 14 15
            //
            // This puts the 8-ball in the center of row 3 (index 4).
            v.objectBalls = {
                makeBall("1 Ball", 1, BallRuleTag::Solid,  glm::vec3(0.86f, 0.72f, 0.10f)),
                makeBall("9 Ball", 9, BallRuleTag::Stripe, glm::vec3(0.86f, 0.72f, 0.10f)),
                makeBall("2 Ball", 2, BallRuleTag::Solid,  glm::vec3(0.10f, 0.28f, 0.78f)),
                makeBall("3 Ball", 3, BallRuleTag::Solid,  glm::vec3(0.78f, 0.10f, 0.10f)),
                makeBall("8 Ball", 8, BallRuleTag::Eight,  glm::vec3(0.05f, 0.05f, 0.06f)),
                makeBall("10 Ball",10, BallRuleTag::Stripe,glm::vec3(0.10f, 0.28f, 0.78f)),
                makeBall("11 Ball",11, BallRuleTag::Stripe,glm::vec3(0.78f, 0.10f, 0.10f)),
                makeBall("4 Ball", 4, BallRuleTag::Solid,  glm::vec3(0.42f, 0.12f, 0.55f)),
                makeBall("5 Ball", 5, BallRuleTag::Solid,  glm::vec3(0.88f, 0.42f, 0.08f)),
                makeBall("12 Ball",12, BallRuleTag::Stripe,glm::vec3(0.42f, 0.12f, 0.55f)),
                makeBall("6 Ball", 6, BallRuleTag::Solid,  glm::vec3(0.08f, 0.45f, 0.20f)),
                makeBall("13 Ball",13, BallRuleTag::Stripe,glm::vec3(0.88f, 0.42f, 0.08f)),
                makeBall("14 Ball",14, BallRuleTag::Stripe,glm::vec3(0.08f, 0.45f, 0.20f)),
                makeBall("7 Ball", 7, BallRuleTag::Solid,  glm::vec3(0.45f, 0.06f, 0.06f)),
                makeBall("15 Ball",15, BallRuleTag::Stripe,glm::vec3(0.45f, 0.06f, 0.06f))
            };

            return v;
        }();

        return variant;
    }

    const GameVariantDefinition& nineBallVariant()
    {
        static const GameVariantDefinition variant = []()
        {
            GameVariantDefinition v;
            v.discipline = GameDiscipline::NineBall;
            v.displayName = "9-Ball";

            v.table = TableSpecification{
                .clothWidth = 2.84f,
                .clothDepth = 1.42f,
                .ballRadius = 0.028575f,
                .ballMassKg = 0.17f
            };

            v.cueBall = BallSpawnDefinition{
                .name = "Cue Ball",
                .number = 0,
                .ruleTag = BallRuleTag::Cue,
                .albedo = glm::vec3(0.93f, 0.93f, 0.91f),
                .specularStrength = 0.95f,
                .shininess = 128.0f,
                .isCueBall = true
            };

            v.rack = RackSpecification{
                .pattern = RackPattern::Diamond,
                .apexPosition = glm::vec3(0.0f, v.table.ballRadius, -0.18f),
                .spacingScale = 1.0f
            };

            // Diamond fill order: 1, 2 3, 4 5 6, 7 8, 9
            // 1 at apex, 9 in center of the diamond.
            v.objectBalls = {
                makeBall("1 Ball", 1, BallRuleTag::Numbered, glm::vec3(0.86f, 0.72f, 0.10f)),
                makeBall("2 Ball", 2, BallRuleTag::Numbered, glm::vec3(0.10f, 0.28f, 0.78f)),
                makeBall("3 Ball", 3, BallRuleTag::Numbered, glm::vec3(0.78f, 0.10f, 0.10f)),
                makeBall("4 Ball", 4, BallRuleTag::Numbered, glm::vec3(0.42f, 0.12f, 0.55f)),
                makeBall("9 Ball", 9, BallRuleTag::Numbered, glm::vec3(0.86f, 0.72f, 0.10f)),
                makeBall("5 Ball", 5, BallRuleTag::Numbered, glm::vec3(0.88f, 0.42f, 0.08f)),
                makeBall("6 Ball", 6, BallRuleTag::Numbered, glm::vec3(0.08f, 0.45f, 0.20f)),
                makeBall("7 Ball", 7, BallRuleTag::Numbered, glm::vec3(0.45f, 0.06f, 0.06f)),
                makeBall("8 Ball", 8, BallRuleTag::Numbered, glm::vec3(0.05f, 0.05f, 0.06f))
            };

            return v;
        }();

        return variant;
    }

    std::vector<glm::vec3> buildRackPositions(const GameVariantDefinition& variant)
    {
        switch (variant.rack.pattern)
        {
            case RackPattern::Triangle:
                return buildTriangleRackPositions(
                    variant.objectBalls.size(),
                    variant.table.ballRadius,
                    variant.rack.apexPosition,
                    variant.rack.spacingScale
                );

            case RackPattern::Diamond:
                return buildDiamondRackPositions(
                    variant.objectBalls.size(),
                    variant.table.ballRadius,
                    variant.rack.apexPosition,
                    variant.rack.spacingScale
                );
        }

        return {};
    }
}
