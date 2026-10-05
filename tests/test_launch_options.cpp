#include "core/launch_options.h"

#include <doctest/doctest.h>

#include <stdexcept>
#include <string_view>
#include <vector>

using namespace BilliardsSaloon;

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

TEST_CASE("bad arguments are rejected with a reason")
{
    CHECK_THROWS_AS(static_cast<void>(parse({"--screen", "lobby"})), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(parse({"--capture"})), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(parse({"--capture-frames", "-3"})), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(parse({"--capture-frames", "12x"})), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(parse({"--speed"})), std::invalid_argument);
}
