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
        MatchSetup,
        Locker
    };

    // Development scenarios played out before the first frame, so captures can
    // show states that need a shot first.
    enum class DevScenario
    {
        None,
        Foul,     // 9-ball: a soft break the wrong way; the next player has ball in hand
        Choice,   // 8-ball: an illegal break; the referee asks the other player
        Call,     // 10-ball: a layout after the break, aiming at a called ball
        Replay    // 9-ball: a slow frame-winning pot, then its slow-motion replay
    };

    // Command-line options. Capture options exist for development and
    // documentation: render a screen, save it as PNG, quit.
    struct LaunchOptions
    {
        bool fullscreen {false};
        StartScreen startScreen {StartScreen::Title};
        std::filesystem::path capturePath;   // empty = no capture
        int captureAfterFrames {90};
        DevScenario scenario {DevScenario::None};
        int startCamera {-1};
        bool mute {false};
        std::string opponent;                 // --opponent ID: play against this AI player                    // --mute; captures are always silent                 // --camera: 0 aim, 1 overview, 2 follow, 3 free, 4 broadcast; -1 = default
    };

    // Parses arguments after the program name. Throws std::invalid_argument
    // with a message suitable for the console on unknown or malformed options.
    [[nodiscard]] LaunchOptions parseLaunchOptions(std::span<const std::string_view> arguments);

    [[nodiscard]] std::string launchOptionsUsage();
}
