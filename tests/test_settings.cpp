#include "core/settings.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

using namespace Septhuit;

namespace
{
    std::filesystem::path scratchFile(const std::string& name)
    {
        const std::filesystem::path directory = std::filesystem::path(BS_TEST_SCRATCH_DIR) / "settings";
        std::filesystem::create_directories(directory);
        const std::filesystem::path file = directory / name;
        std::filesystem::remove(file);
        return file;
    }
}

TEST_CASE("settings survive a save and load")
{
    const std::filesystem::path file = scratchFile("roundtrip.json");

    GameSettings settings;
    settings.fullscreen = true;
    settings.vsync = false;
    settings.quality = QualityLevel::High;
    settings.mouseSensitivity = 1.5f;
    settings.uiScale = 1.25f;
    settings.reducedMotion = true;
    settings.aimGuide = 2;
    settings.matchGame = 2;
    settings.raceTo = 7;
    settings.winnerBreaks = true;
    settings.shotClock = 45;
    settings.opponent = "viktor_hale";
    settings.masterVolume = 0.5f;
    settings.crowdVolume = 0.2f;
    settings.equipment.cloth = "tournament_blue";
    settings.equipment.hall = "arena";

    REQUIRE(saveSettings(file, settings));
    CHECK(loadSettings(file) == settings);
    CHECK_FALSE(std::filesystem::exists(file.string() + ".tmp"));
}

TEST_CASE("a missing settings file gives the defaults")
{
    std::string warning;
    CHECK(loadSettings(scratchFile("missing.json"), &warning) == GameSettings{});
    CHECK(warning.empty());
}

TEST_CASE("a corrupt settings file gives the defaults and a warning")
{
    const std::filesystem::path file = scratchFile("corrupt.json");
    std::ofstream(file) << "{ \"vsync\": fals";

    std::string warning;
    CHECK(loadSettings(file, &warning) == GameSettings{});
    CHECK(warning.find("corrupt.json") != std::string::npos);
}

TEST_CASE("out-of-range values are clamped and unknown keys ignored")
{
    const std::filesystem::path file = scratchFile("ranges.json");
    std::ofstream(file) << R"({ "mouseSensitivity": 40, "uiScale": 0.2, "quality": "ultra", "futureOption": 3 })";

    const GameSettings settings = loadSettings(file);
    CHECK(settings.mouseSensitivity == doctest::Approx(MAX_MOUSE_SENSITIVITY));
    CHECK(settings.uiScale == doctest::Approx(MIN_UI_SCALE));
    CHECK(settings.quality == QualityLevel::Balanced);
}

TEST_CASE("the user data directory is a real folder")
{
    const std::filesystem::path directory = userDataDirectory();
    REQUIRE_FALSE(directory.empty());
    CHECK(std::filesystem::is_directory(directory));
}

TEST_CASE("settings from before the rename are carried over once")
{
    const std::filesystem::path root = std::filesystem::path(BS_TEST_SCRATCH_DIR) / "migration";
    std::filesystem::remove_all(root);
    const std::filesystem::path legacy = root / "Billiards Saloon";
    const std::filesystem::path directory = root / "Septhuit";
    std::filesystem::create_directories(legacy);
    GameSettings old;
    old.raceTo = 7;
    REQUIRE(saveSettings(legacy / "settings.json", old));

    CHECK(migrateUserDataDirectory(legacy, directory));
    CHECK(loadSettings(directory / "settings.json").raceTo == 7);
    CHECK(std::filesystem::exists(legacy / "settings.json"));   // the old folder is left alone

    // Once the new folder exists, it is never overwritten.
    GameSettings changed;
    changed.raceTo = 3;
    REQUIRE(saveSettings(directory / "settings.json", changed));
    CHECK_FALSE(migrateUserDataDirectory(legacy, directory));
    CHECK(loadSettings(directory / "settings.json").raceTo == 3);

    // Nothing to migrate from.
    CHECK_FALSE(migrateUserDataDirectory(root / "missing", root / "fresh"));
    CHECK_FALSE(std::filesystem::exists(root / "fresh"));
}
