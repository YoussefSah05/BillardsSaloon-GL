#pragma once

#include <filesystem>
#include <string>

namespace BilliardsSaloon
{
    enum class QualityLevel
    {
        Low,
        Balanced,
        High
    };

    // Player preferences, saved between sessions as versioned JSON.
    struct GameSettings
    {
        static constexpr int CURRENT_VERSION = 1;

        // Video
        bool fullscreen {false};
        bool vsync {true};
        QualityLevel quality {QualityLevel::Balanced};

        // Controls
        float mouseSensitivity {1.0f};   // multiplies mouse aim, spin and stroke
        int aimGuide {1};                // 0 off, 1 ghost ball and short lines, 2 full predicted paths

        // Accessibility
        float uiScale {1.0f};            // 1.0 to 1.5
        bool reducedMotion {false};

        // Last match setup: 0 = 8-ball, 1 = 9-ball, 2 = 10-ball.
        int matchGame {0};
        int raceTo {3};
        bool winnerBreaks {false};

        friend bool operator==(const GameSettings&, const GameSettings&) = default;
    };

    inline constexpr float MIN_MOUSE_SENSITIVITY = 0.25f;
    inline constexpr float MAX_MOUSE_SENSITIVITY = 3.0f;
    inline constexpr float MIN_UI_SCALE = 1.0f;
    inline constexpr float MAX_UI_SCALE = 1.5f;
    inline constexpr int MATCH_GAME_COUNT = 3;
    inline constexpr int AIM_GUIDE_COUNT = 3;
    inline constexpr int MAX_RACE_TO = 15;

    // Brings every value into its allowed range.
    [[nodiscard]] GameSettings sanitized(GameSettings settings);

    // Per-user folder for settings and saves, created if missing:
    //   macOS   ~/Library/Application Support/Billiards Saloon
    //   Windows %APPDATA%\Billiards Saloon
    //   Linux   $XDG_CONFIG_HOME/billiards-saloon (or ~/.config/billiards-saloon)
    // Returns an empty path if no home directory can be found.
    [[nodiscard]] std::filesystem::path userDataDirectory();

    // Missing file: defaults. Unreadable or invalid file: defaults, and
    // outWarning explains why (the game should still start).
    [[nodiscard]] GameSettings loadSettings(const std::filesystem::path& file, std::string* outWarning = nullptr);

    // Writes atomically (temporary file, then rename). Returns false on failure.
    bool saveSettings(const std::filesystem::path& file, const GameSettings& settings);

    [[nodiscard]] const char* qualityName(QualityLevel quality);
}
