#include "gameplay/game_variant.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace BilliardsSaloon;

namespace
{
    // Writes a variant file plus a valid table file into a fresh data tree.
    std::filesystem::path writeVariant(const std::string& testName, const std::string& variantJson)
    {
        const std::filesystem::path root = std::filesystem::path(BS_TEST_SCRATCH_DIR) / testName;
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "variants");
        std::filesystem::create_directories(root / "tables");

        std::ofstream(root / "tables" / "test_table.json") << R"({
            "name": "Test table",
            "clothWidth": 2.54, "clothDepth": 1.27,
            "ballRadius": 0.028575, "ballMassKg": 0.17,
            "pockets": {"cornerRadius": 0.06, "sideRadius": 0.065},
            "cushion": {"restitution": 0.9, "friction": 0.2},
            "ballContact": {"restitution": 0.95, "friction": 0.06},
            "cloth": {"slidingFriction": 0.2, "rollingFriction": 0.01, "spinningFriction": 0.02, "stopSpeed": 0.005}
        })";

        const std::filesystem::path file = root / "variants" / "variant.json";
        std::ofstream(file) << variantJson;
        return file;
    }

    std::string variantJson(const std::string& objectBalls, const std::string& rack = R"({"pattern": "triangle", "apex": [0.0, -0.2]})")
    {
        return R"({
            "discipline": "eight_ball",
            "displayName": "Test",
            "table": "test_table",
            "rack": )" + rack + R"(,
            "cueBall": {"name": "Cue", "number": 0, "rule": "cue", "color": [1, 1, 1]},
            "objectBalls": )" + objectBalls + "}";
    }

    std::string loadError(const std::filesystem::path& file)
    {
        try
        {
            static_cast<void>(loadGameVariant(file));
        }
        catch (const std::runtime_error& error)
        {
            return error.what();
        }
        return {};
    }
}

TEST_CASE("the shipped 8-ball data describes a WPA 9 ft table")
{
    const GameVariantDefinition& variant = eightBallVariant();

    CHECK(variant.discipline == GameDiscipline::EightBall);
    CHECK(variant.displayName == "8-Ball");
    CHECK(variant.rack.pattern == RackPattern::Triangle);
    CHECK(variant.rack.apexPosition.x == doctest::Approx(0.635f));
    CHECK(variant.rack.apexPosition.z == doctest::Approx(0.0f));
    CHECK(variant.rack.apexPosition.y == doctest::Approx(variant.table.ballRadius));

    CHECK(variant.table.clothWidth == doctest::Approx(2.54f));
    CHECK(variant.table.clothDepth == doctest::Approx(1.27f));
    CHECK(variant.table.ballRadius == doctest::Approx(0.028575f));
    CHECK(variant.table.ballMassKg == doctest::Approx(0.170097f));

    // The simulator's geometry follows the table and its pocket data.
    CHECK(variant.table.pocketGeometry.length == doctest::Approx(2.54));
    CHECK(variant.table.pocketGeometry.width == doctest::Approx(1.27));
    CHECK(variant.table.pocketGeometry.cornerPocketWidth == doctest::Approx(0.118));
    CHECK(variant.table.simBall.R == doctest::Approx(0.028575));
    CHECK(variant.table.simBall.e_c == doctest::Approx(0.85));
    CHECK(variant.table.cornerPocketRadius == doctest::Approx(0.090f));
    CHECK(variant.table.sidePocketRadius == doctest::Approx(0.080f));
    CHECK(variant.table.physics.cushionRestitution == doctest::Approx(0.92f));
    CHECK(variant.table.physics.slidingFriction == doctest::Approx(0.20f));
    CHECK(variant.table.physics.rollingFriction == doctest::Approx(0.010f));

    CHECK(variant.cueBall.isCueBall);
    CHECK(variant.cueBall.specularStrength == doctest::Approx(0.95f));

    REQUIRE(variant.objectBalls.size() == 15);
    CHECK(variant.objectBalls[4].number == 8);
    for (const BallSpawnDefinition& ball : variant.objectBalls)
    {
        const BallRuleTag expected =
            (ball.number == 8) ? BallRuleTag::Eight
            : (ball.number < 8) ? BallRuleTag::Solid
            : BallRuleTag::Stripe;
        CHECK(ball.ruleTag == expected);
        CHECK(ball.specularStrength == doctest::Approx(0.92f));
    }
}

TEST_CASE("the shipped 9-ball data racks the 1 at the apex and the 9 in the middle")
{
    const GameVariantDefinition& variant = nineBallVariant();

    CHECK(variant.discipline == GameDiscipline::NineBall);
    CHECK(variant.rack.pattern == RackPattern::Diamond);
    REQUIRE(variant.objectBalls.size() == 9);
    CHECK(variant.objectBalls.front().number == 1);
    CHECK(variant.objectBalls[4].number == 9);
}

TEST_CASE("a valid custom variant loads with its referenced table")
{
    const std::filesystem::path file = writeVariant("valid", variantJson(
        R"([{"name": "1", "number": 1, "rule": "solid", "color": [1, 0, 0]}])"
    ));

    const GameVariantDefinition variant = loadGameVariant(file);
    CHECK(variant.table.name == "Test table");
    CHECK(variant.table.clothWidth == doctest::Approx(2.54f));
    CHECK(variant.rack.apexPosition.y == doctest::Approx(0.028575f));
    REQUIRE(variant.objectBalls.size() == 1);
    CHECK(variant.objectBalls[0].shininess == doctest::Approx(128.0f));
}

TEST_CASE("invalid variant data is rejected with the file and reason")
{
    SUBCASE("malformed JSON")
    {
        const std::filesystem::path file = writeVariant("malformed", "{ not json");
        const std::string error = loadError(file);
        CHECK(error.find("variant.json") != std::string::npos);
    }

    SUBCASE("missing field")
    {
        const std::filesystem::path file = writeVariant("missing", R"({"discipline": "eight_ball"})");
        CHECK(loadError(file).find("displayName") != std::string::npos);
    }

    SUBCASE("duplicate ball number")
    {
        const std::filesystem::path file = writeVariant("duplicate", variantJson(
            R"([{"name": "a", "number": 3, "rule": "solid", "color": [1, 0, 0]},
                {"name": "b", "number": 3, "rule": "stripe", "color": [0, 1, 0]}])"
        ));
        CHECK(loadError(file).find("used twice") != std::string::npos);
    }

    SUBCASE("unknown rule")
    {
        const std::filesystem::path file = writeVariant("rule", variantJson(
            R"([{"name": "a", "number": 3, "rule": "spotted", "color": [1, 0, 0]}])"
        ));
        CHECK(loadError(file).find("spotted") != std::string::npos);
    }

    SUBCASE("too many balls for a diamond")
    {
        std::string balls = "[";
        for (int i = 1; i <= 10; ++i)
        {
            balls += std::string(i > 1 ? "," : "") + R"({"name": "b", "number": )" + std::to_string(i) +
                     R"(, "rule": "numbered", "color": [1, 1, 1]})";
        }
        balls += "]";

        const std::filesystem::path file = writeVariant("diamond", variantJson(
            balls, R"({"pattern": "diamond", "apex": [0.0, -0.2]})"
        ));
        CHECK(loadError(file).find("diamond") != std::string::npos);
    }

    SUBCASE("missing table file")
    {
        const std::filesystem::path file = writeVariant("notable", variantJson(
            R"([{"name": "1", "number": 1, "rule": "solid", "color": [1, 0, 0]}])"
        ));
        std::filesystem::remove(file.parent_path().parent_path() / "tables" / "test_table.json");
        CHECK(loadError(file).find("test_table.json") != std::string::npos);
    }
}
