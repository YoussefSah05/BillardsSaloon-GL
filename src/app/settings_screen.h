#pragma once

#include "core/settings.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <functional>
#include <string>

namespace Rml
{
    class ElementDocument;
}

namespace BilliardsSaloon
{
    class UiSystem;

    struct SettingsScreenActions
    {
        // Called with the full settings after every change.
        std::function<void(const GameSettings&)> apply;
        std::function<void()> back;
    };

    // The settings screen (assets/ui/settings.rml). Each row cycles through its
    // values with click/Enter (forward) or Left/Right.
    class SettingsScreen
    {
    public:
        SettingsScreen(UiSystem& ui, const GameSettings& initial, SettingsScreenActions actions);

        void setVisible(bool visible);
        [[nodiscard]] bool isVisible() const;

        // Replace the shown values, e.g. after F11 toggled fullscreen.
        void setSettings(const GameSettings& settings);
        [[nodiscard]] const GameSettings& settings() const { return m_settings; }

    private:
        void change(int row, int direction);
        void refreshAll();

        GameSettings m_settings;
        SettingsScreenActions m_actions;
        Rml::DataModelHandle m_model;
        Rml::ElementDocument* m_document {nullptr};
    };
}
