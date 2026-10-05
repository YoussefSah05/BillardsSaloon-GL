#include "gameplay/game_variant.h"

#include "core/asset_paths.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>

namespace BilliardsSaloon
{
    namespace
    {
        using Json = nlohmann::json;

        [[noreturn]] void failData(const std::filesystem::path& file, const std::string& message)
        {
            throw std::runtime_error("Invalid game data in " + file.string() + ": " + message);
        }

        Json readJsonFile(const std::filesystem::path& file)
        {
            std::ifstream stream(file);
            if (!stream)
            {
                throw std::runtime_error("Cannot open game data file: " + file.string());
            }

            try
            {
                return Json::parse(stream);
            }
            catch (const Json::parse_error& error)
            {
                failData(file, error.what());
            }
        }

        glm::vec3 readColor(const Json& value)
        {
            if (!value.is_array() || (value.size() != 3))
            {
                throw std::invalid_argument("\"color\" must be an array of three numbers");
            }

            return glm::vec3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
        }

        BallRuleTag readRuleTag(const std::string& rule)
        {
            if (rule == "cue") return BallRuleTag::Cue;
            if (rule == "solid") return BallRuleTag::Solid;
            if (rule == "stripe") return BallRuleTag::Stripe;
            if (rule == "eight") return BallRuleTag::Eight;
            if (rule == "numbered") return BallRuleTag::Numbered;
            if (rule == "red") return BallRuleTag::Red;
            if (rule == "color") return BallRuleTag::Color;
            throw std::invalid_argument("unknown ball rule \"" + rule + "\"");
        }

        GameDiscipline readDiscipline(const std::string& discipline)
        {
            if (discipline == "eight_ball") return GameDiscipline::EightBall;
            if (discipline == "nine_ball") return GameDiscipline::NineBall;
            throw std::invalid_argument("unknown discipline \"" + discipline + "\"");
        }

        RackPattern readRackPattern(const std::string& pattern)
        {
            if (pattern == "triangle") return RackPattern::Triangle;
            if (pattern == "diamond") return RackPattern::Diamond;
            throw std::invalid_argument("unknown rack pattern \"" + pattern + "\"");
        }

        BallSpawnDefinition readBall(const Json& value)
        {
            BallSpawnDefinition ball;
            ball.name = value.at("name").get<std::string>();
            ball.number = value.at("number").get<int>();
            ball.ruleTag = readRuleTag(value.at("rule").get<std::string>());
            ball.albedo = readColor(value.at("color"));
            ball.specularStrength = value.value("specularStrength", 0.92f);
            ball.shininess = value.value("shininess", 128.0f);
            ball.isCueBall = (ball.ruleTag == BallRuleTag::Cue);
            return ball;
        }

        void validateVariant(const GameVariantDefinition& variant)
        {
            if (!variant.cueBall.isCueBall)
            {
                throw std::invalid_argument("\"cueBall\" must use rule \"cue\"");
            }

            if (variant.objectBalls.empty())
            {
                throw std::invalid_argument("\"objectBalls\" must not be empty");
            }

            if ((variant.rack.pattern == RackPattern::Diamond) && (variant.objectBalls.size() > 9))
            {
                throw std::invalid_argument("a diamond rack holds at most 9 balls");
            }

            std::set<int> numbers {variant.cueBall.number};
            for (const BallSpawnDefinition& ball : variant.objectBalls)
            {
                if (ball.isCueBall)
                {
                    throw std::invalid_argument("object ball \"" + ball.name + "\" uses rule \"cue\"");
                }

                if (!numbers.insert(ball.number).second)
                {
                    throw std::invalid_argument("ball number " + std::to_string(ball.number) + " is used twice");
                }
            }
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

    TableSpecification loadTableSpecification(const std::filesystem::path& tableFile)
    {
        const Json json = readJsonFile(tableFile);

        try
        {
            TableSpecification table;
            table.name = json.at("name").get<std::string>();
            table.clothWidth = json.at("clothWidth").get<float>();
            table.clothDepth = json.at("clothDepth").get<float>();
            table.ballRadius = json.at("ballRadius").get<float>();
            table.ballMassKg = json.at("ballMassKg").get<float>();

            const Json& pockets = json.at("pockets");
            table.cornerPocketRadius = pockets.at("cornerRadius").get<float>();
            table.sidePocketRadius = pockets.at("sideRadius").get<float>();

            const Json& cushion = json.at("cushion");
            table.physics.cushionRestitution = cushion.at("restitution").get<float>();
            table.physics.cushionFriction = cushion.at("friction").get<float>();

            const Json& ballContact = json.at("ballContact");
            table.physics.ballRestitution = ballContact.at("restitution").get<float>();
            table.physics.ballFriction = ballContact.at("friction").get<float>();

            const Json& cloth = json.at("cloth");
            table.physics.slidingFriction = cloth.at("slidingFriction").get<float>();
            table.physics.rollingFriction = cloth.at("rollingFriction").get<float>();
            table.physics.spinningFriction = cloth.at("spinningFriction").get<float>();
            table.physics.stopSpeed = cloth.at("stopSpeed").get<float>();

            if ((table.clothWidth <= 0.0f) || (table.clothDepth <= 0.0f) ||
                (table.ballRadius <= 0.0f) || (table.ballMassKg <= 0.0f))
            {
                throw std::invalid_argument("table and ball dimensions must be positive");
            }

            return table;
        }
        catch (const std::exception& error)
        {
            failData(tableFile, error.what());
        }
    }

    GameVariantDefinition loadGameVariant(const std::filesystem::path& variantFile)
    {
        const Json json = readJsonFile(variantFile);

        GameVariantDefinition variant;
        std::string tableName;

        try
        {
            variant.discipline = readDiscipline(json.at("discipline").get<std::string>());
            variant.displayName = json.at("displayName").get<std::string>();
            tableName = json.at("table").get<std::string>();

            const Json& rack = json.at("rack");
            variant.rack.pattern = readRackPattern(rack.at("pattern").get<std::string>());
            variant.rack.spacingScale = rack.value("spacingScale", 1.0f);

            const Json& apex = rack.at("apex");
            if (!apex.is_array() || (apex.size() != 2))
            {
                throw std::invalid_argument("\"rack.apex\" must be [x, z]");
            }
            variant.rack.apexPosition = glm::vec3(apex[0].get<float>(), 0.0f, apex[1].get<float>());

            variant.cueBall = readBall(json.at("cueBall"));
            for (const Json& ball : json.at("objectBalls"))
            {
                variant.objectBalls.push_back(readBall(ball));
            }

            validateVariant(variant);
        }
        catch (const std::exception& error)
        {
            failData(variantFile, error.what());
        }

        variant.table = loadTableSpecification(
            variantFile.parent_path().parent_path() / "tables" / (tableName + ".json")
        );

        // Balls rest on the cloth.
        variant.rack.apexPosition.y = variant.table.ballRadius;
        return variant;
    }

    const GameVariantDefinition& eightBallVariant()
    {
        static const GameVariantDefinition variant =
            loadGameVariant(resolveAssetPath("data/variants/eight_ball.json"));
        return variant;
    }

    const GameVariantDefinition& nineBallVariant()
    {
        static const GameVariantDefinition variant =
            loadGameVariant(resolveAssetPath("data/variants/nine_ball.json"));
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
