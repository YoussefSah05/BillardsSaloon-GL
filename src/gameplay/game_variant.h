#pragma once

#include "scene/components.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace BilliardsSaloon
{
    enum class GameDiscipline
    {
        EightBall,
        NineBall
    };

    enum class RackPattern
    {
        Triangle,
        Diamond
    };

    struct TableSpecification
    {
        float clothWidth {2.84f};
        float clothDepth {1.42f};
        float ballRadius {0.028575f};
        float ballMassKg {0.17f};
    };

    struct BallSpawnDefinition
    {
        std::string name;
        int number {0};
        BallRuleTag ruleTag {BallRuleTag::Numbered};
        glm::vec3 albedo {1.0f, 1.0f, 1.0f};
        float specularStrength {0.90f};
        float shininess {128.0f};
        bool isCueBall {false};
    };

    struct RackSpecification
    {
        RackPattern pattern {RackPattern::Triangle};

        // Position of the apex ball for triangle and diamond racks.
        glm::vec3 apexPosition {0.0f, 0.028575f, -0.20f};

        // 1.0 means tangent balls with no artificial gap scaling.
        float spacingScale {1.0f};
    };

    struct GameVariantDefinition
    {
        GameDiscipline discipline {GameDiscipline::EightBall};
        std::string displayName {"8-Ball"};

        TableSpecification table;
        BallSpawnDefinition cueBall;
        std::vector<BallSpawnDefinition> objectBalls;
        RackSpecification rack;
    };

    const GameVariantDefinition& eightBallVariant();
    const GameVariantDefinition& nineBallVariant();
}