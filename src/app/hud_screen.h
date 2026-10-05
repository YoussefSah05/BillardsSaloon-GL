#pragma once

#include "gameplay/game_variant.h"
#include "gameplay/match_session.h"
#include "rules/referee.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <array>
#include <string>
#include <vector>

namespace Rml
{
    class ElementDocument;
}

namespace BilliardsSaloon
{
    class UiSystem;

    // A ball icon in the scorebug tray.
    struct HudBall
    {
        std::string fill;    // body colour (ivory for stripes)
        std::string color;   // ball colour
        bool stripe {false};
        bool on {false};     // the ball to hit next (9-ball, 10-ball)
    };

    // Everything the HUD shows that changes during play, gathered each frame.
    struct HudSnapshot
    {
        std::array<std::string, 2> playerNames {"PLAYER 1", "PLAYER 2"};
        GameDiscipline game {GameDiscipline::EightBall};
        std::array<Rules::Group, 2> groups {Rules::Group::None, Rules::Group::None};
        std::array<std::vector<int>, 2> remainingBalls;   // 8-ball: numbers left per player's group
        std::vector<int> tableBalls;                      // 9/10-ball: balls left, ascending
        std::array<int, 2> frames {0, 0};
        int raceTo {1};
        std::array<int, 2> fouls {0, 0};                  // consecutive fouls (9/10-ball)
        int activePlayer {0};
        std::string discipline;
        std::string cameraLabel;

        bool aiming {false};
        bool placing {false};
        bool placementValid {true};
        bool canPlace {false};                            // ball in hand, not yet shot
        bool behindHeadString {false};
        std::string callText;                             // "8 · FOOT LEFT", empty when no call
        bool calling {false};
        bool pushOutAvailable {false};
        bool pushOutDeclared {false};
        bool replaying {false};
        bool replaySlow {false};
        bool clockEnabled {false};
        bool clockRunning {false};
        float clockSeconds {0.0f};
        std::array<bool, 2> extensions {true, true};
        bool canReplay {false};
        std::string aiThinking;                           // the computer player's name while it decides

        float power01 {0.0f};
        float strikeRight01 {0.0f};
        float strikeForward01 {0.0f};
        float elevationDegrees {0.0f};
        bool gamepadPrompts {false};
        bool showPrompts {true};
    };

    [[nodiscard]] std::string pocketName(const glm::vec3& pocketPosition);

    // The in-match HUD (assets/ui/hud.rml): scorebug, ball tray, power meter,
    // spin widget, control prompts, referee banners and lower thirds.
    class HudScreen
    {
    public:
        HudScreen(UiSystem& ui, const GameVariantDefinition& variant);

        // Ball colours for the trays; call when the discipline changes.
        void setVariant(const GameVariantDefinition& variant);

        void setVisible(bool visible);

        // Pushes the latest state to the document; deltaTimeSeconds runs the banner timers.
        void update(const HudSnapshot& snapshot, float deltaTimeSeconds);

        // Shows the referee banner and lower third for a resolved shot.
        void announce(const ShotOutcome& outcome, const HudSnapshot& snapshot);

        // Remembers the power of the shot just played, shown as a marker on the meter.
        void markShotPower(float power01);

        // Clears banners, e.g. when a new match starts.
        void clearAnnouncements();

    private:
        void showBanner(const char* kind, const std::string& text, bool foul, bool legal, float seconds);
        void showLowerThird(const std::string& tag, const std::string& title, const std::string& sub, float seconds);
        void setTray(int tray, const std::vector<int>& numbers, int onBall);

        Rml::DataModelHandle m_model;
        Rml::ElementDocument* m_document {nullptr};
        std::array<HudBall, 16> m_ballStyles {};

        // Bound variables.
        std::string m_names[2];
        std::string m_groupLabels[2];
        std::string m_frames[2];
        std::vector<HudBall> m_trays[3];   // player 1, player 2, table (rotation)
        bool m_groupsAssigned {false};
        bool m_rotation {false};
        bool m_showFrames {false};
        std::string m_race;
        int m_active {0};
        std::string m_discipline;
        std::string m_cameraLabel;
        bool m_aiming {false};
        bool m_placing {false};
        bool m_placementValid {true};
        bool m_canPlace {false};
        std::string m_placeText;
        bool m_calling {false};
        std::string m_callText;
        bool m_pushAvailable {false};
        bool m_pushDeclared {false};
        bool m_replaying {false};
        bool m_clockVisible {false};
        bool m_clockLow {false};
        bool m_clockExtend {false};
        std::string m_clockText;
        bool m_extension[2] {true, true};
        bool m_replaySlow {false};
        bool m_canReplay {false};
        std::string m_aiThinking;
        bool m_gamepad {false};
        bool m_showPrompts {true};
        float m_power {0.0f};
        float m_lastPower {0.0f};
        float m_strikeRight {0.0f};
        float m_strikeForward {0.0f};
        std::string m_elevation;

        bool m_bannerVisible {false};
        bool m_bannerFoul {false};
        bool m_bannerLegal {false};
        std::string m_bannerKind;
        std::string m_bannerText;
        float m_bannerTimer {0.0f};

        bool m_lowerVisible {false};
        std::string m_lowerTag;
        std::string m_lowerTitle;
        std::string m_lowerSub;
        float m_lowerTimer {0.0f};

        std::vector<int> m_trayNumbers[3];
        int m_trayOn[3] {-1, -1, -1};
    };
}
