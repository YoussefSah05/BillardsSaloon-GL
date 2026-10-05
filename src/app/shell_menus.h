#pragma once

#include <RmlUi/Core/DataModelHandle.h>

#include <functional>

namespace Rml
{
    class ElementDocument;
}

namespace BilliardsSaloon
{
    class UiSystem;

    enum class MenuScreen
    {
        None,
        Main,
        Pause
    };

    // What the menus can ask the game to do. Called from inside UI event
    // handling, so actions should only change state, never destroy the UI.
    struct ShellMenuActions
    {
        std::function<void()> startMatch;
        std::function<void()> toggleFullscreen;
        std::function<void()> quit;
        std::function<void()> resume;
        std::function<void()> restartRack;
        std::function<void()> returnToMainMenu;
        std::function<bool()> isFullscreen;
    };

    // The main and pause menus (assets/ui/*.rml) and their "shell" data model.
    class ShellMenus
    {
    public:
        ShellMenus(UiSystem& ui, ShellMenuActions actions);

        void show(MenuScreen screen);
        [[nodiscard]] MenuScreen shown() const { return m_shown; }

        // Re-read values shown in the menus (e.g. after fullscreen changes).
        void refresh();

    private:
        ShellMenuActions m_actions;
        Rml::DataModelHandle m_model;
        Rml::ElementDocument* m_mainMenu {nullptr};
        Rml::ElementDocument* m_pauseMenu {nullptr};
        MenuScreen m_shown {MenuScreen::None};
    };
}
