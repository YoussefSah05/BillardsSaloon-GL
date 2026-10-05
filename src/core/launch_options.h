#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace BilliardsSaloon
{
    enum class StartScreen
    {
        Title,
        MainMenu,
        Gameplay,
        Pause,
        Settings,
        MatchSetup
    };

    // Development scenarios played out before the first frame, so captures can
    // show states that need a shot first.
    enum class DevScenario
    {
        None,
        Foul,     // 9-ball: a soft break the wrong way; the next player has ball in hand
        Choice,   // 8-ball: an illegal break; the referee asks the other player
        Call      // 10-ball: a layout after the break, aiming at a called ball
    };

    // Command-line options. Capture options exist for development and
    // documentation: render a screen, save it as PNG, quit.
    struct LaunchOptions
    {
        bool fullscreen {false};
        StartScreen startScreen {StartScreen::Title};
        std::filesystem::path capturePath;   // empty = no capture
        int captureAfterFrames {90};
        bool legacyPhysics {false};           // --physics legacy: prototype solver
        DevScenario scenario {DevScenario::None};
    };

    // Parses arguments after the program name. Throws std::invalid_argument
    // with a message suitable for the console on unknown or malformed options.
    [[nodiscard]] LaunchOptions parseLaunchOptions(std::span<const std::string_view> arguments);

    [[nodiscard]] std::string launchOptionsUsage();
}
