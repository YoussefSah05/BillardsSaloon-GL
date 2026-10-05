#pragma once

#include "scene/components.h"
#include "sim/motion.h"
#include "sim/table.h"

#include <glm/glm.hpp>

#include <filesystem>
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

    // Contact and cloth coefficients. Gameplay-tuned, guided by common
    // pool-physics references (see TableBoundsComponent for ranges).
    struct TablePhysicsSpecification
    {
        float cushionRestitution {0.92f};
        float cushionFriction {0.14f};
        float ballRestitution {0.96f};
        float ballFriction {0.05f};
        float slidingFriction {0.20f};
        float rollingFriction {0.010f};
        float spinningFriction {0.015f};
        float stopSpeed {0.006f};
    };

    struct TableSpecification
    {
        std::string name {"10 ft table"};
        float clothWidth {2.84f};
        float clothDepth {1.42f};
        float ballRadius {0.028575f};
        float ballMassKg {0.17f};
        float cornerPocketRadius {0.090f};
        float sidePocketRadius {0.080f};
        TablePhysicsSpecification physics {};

        // Event-based simulator: pocket and cushion geometry and ball
        // coefficients (lengths, ball size and mass come from the fields above).
        Sim::PocketTableSpec pocketGeometry {};
        Sim::BallParams simBall {};
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

    // Loads a variant from JSON. The "table" field names a file in the
    // sibling tables/ directory (data/variants/x.json -> data/tables/<table>.json).
    // Throws std::runtime_error naming the file and the problem on invalid data.
    [[nodiscard]] GameVariantDefinition loadGameVariant(const std::filesystem::path& variantFile);
    [[nodiscard]] TableSpecification loadTableSpecification(const std::filesystem::path& tableFile);

    // Built-in variants from assets/data/variants, loaded once on first use.
    const GameVariantDefinition& eightBallVariant();
    const GameVariantDefinition& nineBallVariant();

    // Rest positions of the object balls, in the order of variant.objectBalls.
    [[nodiscard]] std::vector<glm::vec3> buildRackPositions(const GameVariantDefinition& variant);
}