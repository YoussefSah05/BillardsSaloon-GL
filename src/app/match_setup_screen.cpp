#include "app/match_setup_screen.h"

#include "ui/ui_system.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <utility>

namespace BilliardsSaloon
{
    namespace
    {
        enum Row
        {
            ROW_GAME = 0,
            ROW_RACE,
            ROW_BREAK,
            ROW_CLOCK
        };

        constexpr std::array<int, 4> CLOCK_STEPS {0, 30, 45, 60};

        constexpr std::array<int, 8> RACE_STEPS {1, 2, 3, 5, 7, 9, 11, 13};
        constexpr std::array<GameDiscipline, 3> GAMES {GameDiscipline::EightBall, GameDiscipline::NineBall, GameDiscipline::TenBall};

        const char* aboutGame(GameDiscipline game)
        {
            switch (game)
            {
                case GameDiscipline::EightBall:
                    return "Solids or stripes, then the 8 into a called pocket. WPA rules.";
                case GameDiscipline::NineBall:
                    return "Lowest ball first; the 9 wins on any legal shot. Push-out after the break, three fouls lose.";
                case GameDiscipline::TenBall:
                    return "Lowest ball first and every shot called. The 10 wins only when called.";
            }
            return "";
        }

        template <typename T, std::size_t N>
        std::size_t indexOf(const std::array<T, N>& values, const T& value)
        {
            const auto it = std::find(values.begin(), values.end(), value);
            return (it == values.end()) ? 0 : static_cast<std::size_t>(it - values.begin());
        }

        template <std::size_t N>
        std::size_t stepIndex(std::size_t index, int direction)
        {
            return (index + N + static_cast<std::size_t>(direction + static_cast<int>(N))) % N;
        }
    }

    const char* disciplineLabel(GameDiscipline game)
    {
        switch (game)
        {
            case GameDiscipline::EightBall:
                return "8-BALL";
            case GameDiscipline::NineBall:
                return "9-BALL";
            case GameDiscipline::TenBall:
                return "10-BALL";
        }
        return "";
    }

    MatchSetupScreen::MatchSetupScreen(UiSystem& ui, const MatchSetup& initial, MatchSetupActions actions)
        : m_setup(initial)
        , m_actions(std::move(actions))
    {
        Rml::DataModelConstructor model = ui.context().CreateDataModel("setup");
        if (!model)
        {
            throw std::runtime_error("Could not create the match setup data model.");
        }

        model.BindFunc("game", [this](Rml::Variant& v) { v = Rml::String(disciplineLabel(m_setup.game)); });
        model.BindFunc("about", [this](Rml::Variant& v) { v = Rml::String(aboutGame(m_setup.game)); });
        model.BindFunc("race", [this](Rml::Variant& v)
        {
            v = (m_setup.raceTo == 1) ? Rml::String("SINGLE FRAME") : std::to_string(m_setup.raceTo) + " FRAMES";
        });
        model.BindFunc("breaks", [this](Rml::Variant& v)
        {
            v = Rml::String((m_setup.breakOrder == Rules::BreakOrder::Alternate) ? "ALTERNATE" : "WINNER BREAKS");
        });
        model.BindFunc("clock", [this](Rml::Variant& v)
        {
            v = (m_setup.shotClock == 0) ? Rml::String("OFF") : std::to_string(m_setup.shotClock) + " SECONDS";
        });
        model.Bind("gamepad", &m_gamepad);

        model.BindEventCallback("cycle", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments)
        {
            if (arguments.size() == 2)
            {
                change(arguments[0].Get<int>(), arguments[1].Get<int>());
            }
        });
        model.BindEventCallback("key", [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments)
        {
            if (arguments.size() != 2)
            {
                return;
            }
            const int key = arguments[1].Get<int>();
            if ((key == Rml::Input::KI_LEFT) || (key == Rml::Input::KI_RIGHT))
            {
                change(arguments[0].Get<int>(), (key == Rml::Input::KI_LEFT) ? -1 : 1);
                event.StopPropagation();
            }
        });
        model.BindEventCallback("start", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            if (m_actions.start)
            {
                m_actions.start(m_setup);
            }
        });
        model.BindEventCallback("back", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            if (m_actions.back)
            {
                m_actions.back();
            }
        });

        m_model = model.GetModelHandle();
        m_document = &ui.loadDocument("ui/match_setup.rml");
    }

    void MatchSetupScreen::setVisible(bool visible)
    {
        if (visible == m_document->IsVisible())
        {
            return;
        }
        if (visible)
        {
            m_document->Show(Rml::ModalFlag::None, Rml::FocusFlag::Auto);
        }
        else
        {
            m_document->Hide();
        }
    }

    bool MatchSetupScreen::isVisible() const
    {
        return m_document->IsVisible();
    }

    void MatchSetupScreen::setGamepadPrompts(bool gamepad)
    {
        m_gamepad = gamepad;
        m_model.DirtyVariable("gamepad");
    }

    void MatchSetupScreen::change(int row, int direction)
    {
        switch (row)
        {
            case ROW_GAME:
                m_setup.game = GAMES[stepIndex<GAMES.size()>(indexOf(GAMES, m_setup.game), direction)];
                break;
            case ROW_RACE:
                m_setup.raceTo = RACE_STEPS[stepIndex<RACE_STEPS.size()>(indexOf(RACE_STEPS, m_setup.raceTo), direction)];
                break;
            case ROW_BREAK:
                m_setup.breakOrder = (m_setup.breakOrder == Rules::BreakOrder::Alternate)
                    ? Rules::BreakOrder::WinnerBreaks
                    : Rules::BreakOrder::Alternate;
                break;
            case ROW_CLOCK:
                m_setup.shotClock = CLOCK_STEPS[stepIndex<CLOCK_STEPS.size()>(indexOf(CLOCK_STEPS, m_setup.shotClock), direction)];
                break;
            default:
                return;
        }

        for (const char* name : {"game", "about", "race", "breaks", "clock"})
        {
            m_model.DirtyVariable(name);
        }
    }
}
