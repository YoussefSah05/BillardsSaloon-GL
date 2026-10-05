#pragma once

#include <RmlUi/Core/DataModelHandle.h>

#include <array>
#include <functional>
#include <string>
#include <vector>

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
        std::function<void()> openLocker;
        std::function<void()> quit;
        std::function<void()> resume;
        std::function<void()> restartRack;
        std::function<void()> returnToMainMenu;
        std::function<void()> frameContinue;    // next frame, or a rematch once the match is over
    };

    // The frame-over card's text.
    struct FrameResultText
    {
        std::string eyebrow;     // "FRAME OVER" or "MATCH OVER"
        std::string headline;    // "PLAYER 1 WINS THE FRAME"
        std::string detail;
        std::string score;       // "2 – 1 · RACE TO 3", empty for a single frame
        std::string primary;     // "NEXT FRAME" or "REMATCH"
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

        void setFrameResult(const FrameResultText& text);

        // The referee's question to a player (up to three answers), shown over
        // the table. onPick gets the index of the chosen answer.
        void showChoice(const std::string& player, const std::string& title, const std::string& detail,
                        const std::vector<std::string>& options, std::function<void(int)> onPick);
        [[nodiscard]] bool choiceOpen() const;
        void hideChoice();

    private:
        ShellMenuActions m_actions;
        Rml::DataModelHandle m_model;
        Rml::ElementDocument* m_title {nullptr};
        Rml::ElementDocument* m_mainMenu {nullptr};
        Rml::ElementDocument* m_pauseMenu {nullptr};
        Rml::ElementDocument* m_frameOver {nullptr};
        Rml::ElementDocument* m_confirm {nullptr};
        Rml::ElementDocument* m_choice {nullptr};
        std::string m_choicePlayer;
        std::string m_choiceTitle;
        std::string m_choiceDetail;
        std::array<std::string, 3> m_choiceOptions;
        int m_choiceCount {0};
        std::function<void(int)> m_onPick;
        std::string m_confirmTitle;
        std::string m_confirmDetail;
        std::string m_confirmAction;
        std::function<void()> m_onConfirm;
        FrameResultText m_frame;
        std::string m_version;
        bool m_gamepad {false};
        MenuScreen m_shown {MenuScreen::None};
    };
}
