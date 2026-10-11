#include "core/launch_options.h"

#include <doctest/doctest.h>

#include <stdexcept>
#include <string_view>
#include <vector>

using namespace Septhuit;

namespace
{
    LaunchOptions parse(std::vector<std::string_view> arguments)
    {
        return parseLaunchOptions(arguments);
    }
}

TEST_CASE("no arguments starts windowed on the main menu")
{
    const LaunchOptions options = parse({});
    CHECK_FALSE(options.fullscreen);
    CHECK(options.startScreen == StartScreen::Title);
    CHECK(options.capturePath.empty());
}

TEST_CASE("capture options are read in any order")
{
    const LaunchOptions options = parse({"--capture-frames", "30", "--screen", "pause", "--capture", "out.png", "--fullscreen"});
    CHECK(options.fullscreen);
    CHECK(options.startScreen == StartScreen::Pause);
    CHECK(options.capturePath == "out.png");
    CHECK(options.captureAfterFrames == 30);
}

TEST_CASE("recording saves frames to a directory")
{
    CHECK(parse({"--record", "frames"}).recordFramesPerSecond == 60);
    const LaunchOptions options = parse({"--record", "frames", "--record-fps", "120", "--capture-frames", "240"});
    CHECK(options.recordDirectory == "frames");
    CHECK(options.recordFramesPerSecond == 120);
    CHECK(options.captureAfterFrames == 240);
    CHECK(options.capturePath.empty());
    CHECK_THROWS_AS(static_cast<void>(parse({"--record"})), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(parse({"--record-fps", "0"})), std::invalid_argument);
}

TEST_CASE("bad arguments are rejected with a reason")
{
    CHECK_THROWS_AS(static_cast<void>(parse({"--screen", "lobby"})), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(parse({"--capture"})), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(parse({"--capture-frames", "-3"})), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(parse({"--capture-frames", "12x"})), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(parse({"--speed"})), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(parse({"--physics", "magic"})), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(parse({"--physics", "legacy"})), std::invalid_argument);   // removed in 0.4.0
}

TEST_CASE("development scenarios start in the game")
{
    const LaunchOptions options = parse({"--scenario", "call"});
    CHECK(options.scenario == DevScenario::Call);
    CHECK(parse({"--scenario", "break"}).scenario == DevScenario::Break);
    CHECK(options.startScreen == StartScreen::Gameplay);
    CHECK(parse({"--screen", "setup"}).startScreen == StartScreen::MatchSetup);
    CHECK_THROWS_AS(static_cast<void>(parse({"--scenario", "trickshot"})), std::invalid_argument);
}
