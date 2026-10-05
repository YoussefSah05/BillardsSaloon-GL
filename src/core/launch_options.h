#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace BilliardsSaloon
{
    enum class StartScreen
    {
        MainMenu,
        Gameplay,
        Pause
    };

    // Command-line options. Capture options exist for development and
    // documentation: render a screen, save it as PNG, quit.
    struct LaunchOptions
    {
        bool fullscreen {false};
        StartScreen startScreen {StartScreen::MainMenu};
        std::filesystem::path capturePath;   // empty = no capture
        int captureAfterFrames {90};
    };

    // Parses arguments after the program name. Throws std::invalid_argument
    // with a message suitable for the console on unknown or malformed options.
    [[nodiscard]] LaunchOptions parseLaunchOptions(std::span<const std::string_view> arguments);

    [[nodiscard]] std::string launchOptionsUsage();
}
