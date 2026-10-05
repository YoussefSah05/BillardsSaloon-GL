#include "app/settings_screen.h"

#include "ui/ui_system.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <utility>

namespace BilliardsSaloon
{
    namespace
    {
        enum Row
        {
            ROW_FULLSCREEN = 0,
            ROW_VSYNC,
            ROW_QUALITY,
            ROW_SENSITIVITY,
            ROW_UI_SCALE,
            ROW_REDUCED_MOTION,
            ROW_AIM_GUIDE
        };

        const char* aimGuideName(int guide)
        {
            switch (guide)
            {
                case 0: return "OFF";
                case 2: return "FULL PATH";
                default: return "GHOST BALL";
            }
        }

        constexpr std::array<float, 7> SENSITIVITY_STEPS {0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 2.5f};
        constexpr std::array<float, 5> UI_SCALE_STEPS {1.0f, 1.1f, 1.25f, 1.4f, 1.5f};

        const char* onOff(bool value)
        {
            return value ? "ON" : "OFF";
        }

        // Index of the step closest to value.
        template <std::size_t N>
        std::size_t nearestStep(const std::array<float, N>& steps, float value)
        {
            std::size_t best = 0;
            for (std::size_t i = 1; i < N; ++i)
            {
                if (std::abs(steps[i] - value) < std::abs(steps[best] - value))
                {
                    best = i;
                }
            }
            return best;
        }

        // Moves through steps, wrapping around at either end.
        template <std::size_t N>
        float stepValue(const std::array<float, N>& steps, float value, int direction)
        {
            const std::size_t index = nearestStep(steps, value);
            const std::size_t next = (index + N + static_cast<std::size_t>(direction + static_cast<int>(N))) % N;
            return steps[next];
        }

        QualityLevel stepQuality(QualityLevel quality, int direction)
        {
            const int index = (static_cast<int>(quality) + direction + 3) % 3;
            return static_cast<QualityLevel>(index);
        }

        std::string formatted(const char* format, double value)
        {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), format, value);
            return buffer;
        }
    }

    SettingsScreen::SettingsScreen(UiSystem& ui, const GameSettings& initial, SettingsScreenActions actions)
        : m_settings(initial)
        , m_actions(std::move(actions))
    {
        Rml::DataModelConstructor model = ui.context().CreateDataModel("settings");
        if (!model)
        {
            throw std::runtime_error("Could not create the settings data model.");
        }

        model.BindFunc("fullscreen", [this](Rml::Variant& v) { v = Rml::String(onOff(m_settings.fullscreen)); });
        model.BindFunc("vsync", [this](Rml::Variant& v) { v = Rml::String(onOff(m_settings.vsync)); });
        model.BindFunc("quality", [this](Rml::Variant& v)
        {
            std::string name = qualityName(m_settings.quality);
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            v = name;
        });
        model.BindFunc("sensitivity", [this](Rml::Variant& v) { v = formatted("%.2fx", static_cast<double>(m_settings.mouseSensitivity)); });
        model.BindFunc("ui_scale", [this](Rml::Variant& v) { v = formatted("%.0f%%", static_cast<double>(m_settings.uiScale * 100.0f)); });
        model.BindFunc("reduced_motion", [this](Rml::Variant& v) { v = Rml::String(onOff(m_settings.reducedMotion)); });
        model.BindFunc("aim_guide", [this](Rml::Variant& v) { v = Rml::String(aimGuideName(m_settings.aimGuide)); });

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
            if (key == Rml::Input::KI_LEFT)
            {
                change(arguments[0].Get<int>(), -1);
                event.StopPropagation();
            }
            else if (key == Rml::Input::KI_RIGHT)
            {
                change(arguments[0].Get<int>(), 1);
                event.StopPropagation();
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
        m_document = &ui.loadDocument("ui/settings.rml");
    }

    void SettingsScreen::setVisible(bool visible)
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

    void SettingsScreen::setGamepadPrompts(bool gamepad)
    {
        m_gamepad = gamepad;
        m_model.DirtyVariable("gamepad");
    }

    bool SettingsScreen::isVisible() const
    {
        return m_document->IsVisible();
    }

    void SettingsScreen::setSettings(const GameSettings& settings)
    {
        m_settings = settings;
        refreshAll();
    }

    void SettingsScreen::refreshAll()
    {
        for (const char* name : {"fullscreen", "vsync", "quality", "sensitivity", "ui_scale", "reduced_motion", "aim_guide"})
        {
            m_model.DirtyVariable(name);
        }
    }

    void SettingsScreen::change(int row, int direction)
    {
        switch (row)
        {
            case ROW_FULLSCREEN:
                m_settings.fullscreen = !m_settings.fullscreen;
                break;
            case ROW_VSYNC:
                m_settings.vsync = !m_settings.vsync;
                break;
            case ROW_QUALITY:
                m_settings.quality = stepQuality(m_settings.quality, direction);
                break;
            case ROW_SENSITIVITY:
                m_settings.mouseSensitivity = stepValue(SENSITIVITY_STEPS, m_settings.mouseSensitivity, direction);
                break;
            case ROW_UI_SCALE:
                m_settings.uiScale = stepValue(UI_SCALE_STEPS, m_settings.uiScale, direction);
                break;
            case ROW_REDUCED_MOTION:
                m_settings.reducedMotion = !m_settings.reducedMotion;
                break;
            case ROW_AIM_GUIDE:
                m_settings.aimGuide = (m_settings.aimGuide + direction + AIM_GUIDE_COUNT) % AIM_GUIDE_COUNT;
                break;
            default:
                return;
        }

        m_settings = sanitized(m_settings);
        refreshAll();

        if (m_actions.apply)
        {
            m_actions.apply(m_settings);
        }
    }
}
