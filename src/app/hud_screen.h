#pragma once

#include "gameplay/game_variant.h"
#include "gameplay/match_session.h"
#include "gameplay/match_state.h"

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
    };

    // Everything the HUD shows that changes during play, gathered each frame.
    struct HudSnapshot
    {
        std::array<std::string, 2> playerNames {"PLAYER 1", "PLAYER 2"};
        std::array<PlayerTargetGroup, 2> groups {PlayerTargetGroup::None, PlayerTargetGroup::None};
        std::array<std::vector<int>, 2> remainingBalls;   // numbers left per player's group
        int activePlayer {0};
        std::string discipline;
        std::string cameraLabel;
        bool aiming {false};
        float power01 {0.0f};
        float strikeRight01 {0.0f};
        float strikeForward01 {0.0f};
        bool gamepadPrompts {false};
    };

    // The in-match HUD (assets/ui/hud.rml): scorebug, ball tray, power meter,
    // spin widget, control prompts, referee banners and lower thirds.
    class HudScreen
    {
    public:
        HudScreen(UiSystem& ui, const GameVariantDefinition& variant);

        void setVisible(bool visible);

        // Pushes the latest state to the document; deltaTimeSeconds runs the banner timers.
        void update(const HudSnapshot& snapshot, float deltaTimeSeconds);

        // Shows the referee banner and lower third for a resolved shot.
        void announce(const ShotOutcome& outcome, const HudSnapshot& snapshot);

        // Remembers the power of the shot just played, shown as a marker on the meter.
        void markShotPower(float power01);

    private:
        void showBanner(const char* kind, const std::string& text, bool foul, bool legal, float seconds);
        void showLowerThird(const std::string& tag, const std::string& title, const std::string& sub, float seconds);
        void setTray(int player, const std::vector<int>& numbers);

        Rml::DataModelHandle m_model;
        Rml::ElementDocument* m_document {nullptr};
        std::array<HudBall, 16> m_ballStyles {};

        // Bound variables.
        std::string m_names[2];
        std::string m_groupLabels[2];
        std::vector<HudBall> m_trays[2];
        bool m_groupsAssigned {false};
        int m_active {0};
        std::string m_discipline;
        std::string m_cameraLabel;
        bool m_aiming {false};
        bool m_gamepad {false};
        float m_power {0.0f};
        float m_lastPower {0.0f};
        float m_strikeRight {0.0f};
        float m_strikeForward {0.0f};

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

        std::vector<int> m_trayNumbers[2];
    };
}
