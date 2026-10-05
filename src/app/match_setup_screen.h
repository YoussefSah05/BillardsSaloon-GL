#pragma once

#include "gameplay/game_variant.h"
#include "rules/match_score.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <functional>

namespace Rml
{
    class ElementDocument;
}

namespace BilliardsSaloon
{
    class UiSystem;

    struct MatchSetup
    {
        GameDiscipline game {GameDiscipline::EightBall};
        int raceTo {3};
        Rules::BreakOrder breakOrder {Rules::BreakOrder::Alternate};
    };

    struct MatchSetupActions
    {
        std::function<void(const MatchSetup&)> start;
        std::function<void()> back;
    };

    // Quick match setup (assets/ui/match_setup.rml): discipline, race length
    // and break order. Rows cycle like the settings screen.
    class MatchSetupScreen
    {
    public:
        MatchSetupScreen(UiSystem& ui, const MatchSetup& initial, MatchSetupActions actions);

        void setVisible(bool visible);
        [[nodiscard]] bool isVisible() const;
        void setGamepadPrompts(bool gamepad);
        [[nodiscard]] const MatchSetup& setup() const { return m_setup; }

    private:
        void change(int row, int direction);

        MatchSetup m_setup;
        bool m_gamepad {false};
        MatchSetupActions m_actions;
        Rml::DataModelHandle m_model;
        Rml::ElementDocument* m_document {nullptr};
    };

    [[nodiscard]] const char* disciplineLabel(GameDiscipline game);
}
