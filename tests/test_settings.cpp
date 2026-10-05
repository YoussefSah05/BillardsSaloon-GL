#include "core/settings.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

using namespace BilliardsSaloon;

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
    settings.matchGame = 2;
    settings.raceTo = 7;
    settings.winnerBreaks = true;

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
