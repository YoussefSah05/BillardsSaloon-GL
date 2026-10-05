#include "gameplay/equipment.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

using namespace BilliardsSaloon;

TEST_CASE("the shipped equipment catalogue loads with every category filled")
{
    const EquipmentCatalog& catalog = equipmentCatalog();
    CHECK(catalog.cloth.size() >= 3);
    CHECK(catalog.rails.size() >= 3);
    CHECK(catalog.trim.size() >= 2);
    CHECK(catalog.pockets.size() >= 2);
    CHECK(catalog.balls.size() >= 2);
    CHECK(catalog.cues.size() >= 2);
    CHECK(catalog.halls.size() >= 2);

    const BallSetOption& broadcast = findOption(catalog.balls, "broadcast");
    CHECK(broadcast.measleCueBall);
    CHECK(broadcast.colors.size() == 15);
}

TEST_CASE("an unknown id falls back to the first option")
{
    const EquipmentCatalog& catalog = equipmentCatalog();
    CHECK(findOption(catalog.cloth, "tournament_blue").id == "tournament_blue");
    CHECK(findOption(catalog.cloth, "neon_pink").id == catalog.cloth.front().id);
}

TEST_CASE("a broken catalogue is rejected with the file and the reason")
{
    const std::filesystem::path file = std::filesystem::path(BS_TEST_SCRATCH_DIR) / "bad_catalog.json";
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file) << R"({"cloth": [], "rails": [], "trim": [], "pockets": [], "balls": [], "cues": [], "halls": []})";
    CHECK_THROWS_WITH_AS(static_cast<void>(loadEquipmentCatalog(file)), doctest::Contains("cloth"), std::runtime_error);
}
