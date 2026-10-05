#pragma once

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

    enum class MenuScreen
    {
        None,
        Title,
        Main,
        Pause,
        FrameOver
    };

    // What the menus can ask the game to do. Called from inside UI event
    // handling, so actions should only change state, never destroy the UI.
    struct ShellMenuActions
    {
        std::function<void()> startMatch;
        std::function<void()> openSettings;
        std::function<void()> quit;
        std::function<void()> resume;
        std::function<void()> restartRack;
        std::function<void()> returnToMainMenu;
    };

    // The main and pause menus (assets/ui/*.rml) and their "shell" data model.
    class ShellMenus
    {
    public:
        ShellMenus(UiSystem& ui, ShellMenuActions actions);

        void show(MenuScreen screen);
        [[nodiscard]] MenuScreen shown() const { return m_shown; }

        // A modal question over the current menu. onConfirm runs only if the
        // player confirms; Cancel (the focused default) just closes it.
        void askConfirmation(const std::string& title, const std::string& detail,
                             const std::string& action, std::function<void()> onConfirm);
        [[nodiscard]] bool confirmationOpen() const;
        void cancelConfirmation();

        // Switch hint lines between keyboard/mouse and gamepad wording.
        void setGamepadPrompts(bool gamepad);

        // Text for the frame-over card, e.g. "PLAYER 1 WINS THE FRAME".
        void setFrameResult(const std::string& headline, const std::string& detail);

    private:
        ShellMenuActions m_actions;
        Rml::DataModelHandle m_model;
        Rml::ElementDocument* m_title {nullptr};
        Rml::ElementDocument* m_mainMenu {nullptr};
        Rml::ElementDocument* m_pauseMenu {nullptr};
        Rml::ElementDocument* m_frameOver {nullptr};
        Rml::ElementDocument* m_confirm {nullptr};
        std::string m_confirmTitle;
        std::string m_confirmDetail;
        std::string m_confirmAction;
        std::function<void()> m_onConfirm;
        std::string m_frameWinner;
        std::string m_frameDetail;
        std::string m_version;
        bool m_gamepad {false};
        MenuScreen m_shown {MenuScreen::None};
    };
}
