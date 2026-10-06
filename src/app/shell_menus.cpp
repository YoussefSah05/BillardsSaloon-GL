#include "app/shell_menus.h"

#include "ui/ui_system.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <utility>

namespace BilliardsSaloon
{
    namespace
    {
        // Wraps a game action as a data-event callback.
        Rml::DataEventFunc callback(std::function<void()> action)
        {
            return [action = std::move(action)](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
            {
                if (action)
                {
                    action();
                }
            };
        }
    }

    ShellMenus::ShellMenus(UiSystem& ui, ShellMenuActions actions)
        : m_actions(std::move(actions))
    {
        // The model must exist before the documents that use it are loaded.
        Rml::DataModelConstructor model = ui.context().CreateDataModel("shell");
        if (!model)
        {
            throw std::runtime_error("Could not create the shell UI data model.");
        }

        m_version = "v" BS_VERSION;
        model.Bind("version", &m_version);
        model.Bind("gamepad", &m_gamepad);
        model.Bind("confirm_title", &m_confirmTitle);
        model.Bind("confirm_detail", &m_confirmDetail);
        model.Bind("confirm_action", &m_confirmAction);
        model.BindEventCallback("confirm_no", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            cancelConfirmation();
        });
        model.BindEventCallback("confirm_yes", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            std::function<void()> action = std::move(m_onConfirm);
            cancelConfirmation();
            if (action)
            {
                action();
            }
        });
        model.Bind("frame_eyebrow", &m_frame.eyebrow);
        model.Bind("frame_winner", &m_frame.headline);
        model.Bind("frame_detail", &m_frame.detail);
        model.Bind("frame_score", &m_frame.score);
        model.Bind("frame_primary", &m_frame.primary);
        model.BindEventCallback("frame_continue", callback(m_actions.frameContinue));

        model.Bind("choice_player", &m_choicePlayer);
        model.Bind("choice_title", &m_choiceTitle);
        model.Bind("choice_detail", &m_choiceDetail);
        model.Bind("choice_option0", &m_choiceOptions[0]);
        model.Bind("choice_option1", &m_choiceOptions[1]);
        model.Bind("choice_option2", &m_choiceOptions[2]);
        model.Bind("choice_count", &m_choiceCount);
        model.BindEventCallback("choice_pick", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments)
        {
            if (arguments.empty())
            {
                return;
            }
            const int index = arguments[0].Get<int>();
            std::function<void(int)> pick = std::move(m_onPick);
            hideChoice();
            if (pick && (index >= 0) && (index < m_choiceCount))
            {
                pick(index);
            }
        });

        model.BindEventCallback("start_match", callback(m_actions.startMatch));
        model.BindEventCallback("open_settings", callback(m_actions.openSettings));
        model.BindEventCallback("open_locker", callback(m_actions.openLocker));
        model.BindEventCallback("quit", callback(m_actions.quit));
        model.BindEventCallback("resume", callback(m_actions.resume));
        model.BindEventCallback("restart_rack", callback(m_actions.restartRack));
        model.BindEventCallback("main_menu", callback(m_actions.returnToMainMenu));

        m_model = model.GetModelHandle();

        m_title = &ui.loadDocument("ui/title.rml");
        m_mainMenu = &ui.loadDocument("ui/main_menu.rml");
        m_pauseMenu = &ui.loadDocument("ui/pause_menu.rml");
        m_frameOver = &ui.loadDocument("ui/frame_over.rml");
        m_confirm = &ui.loadDocument("ui/confirm.rml");
        m_choice = &ui.loadDocument("ui/choice.rml");
    }

    void ShellMenus::show(MenuScreen screen)
    {
        if (screen == m_shown)
        {
            return;
        }

        m_shown = screen;
        cancelConfirmation();
        m_title->Hide();
        m_mainMenu->Hide();
        m_pauseMenu->Hide();
        m_frameOver->Hide();

        // FocusFlag::Auto focuses the element marked autofocus, so the
        // keyboard and gamepad can navigate straight away.
        if (screen == MenuScreen::Title)
        {
            m_title->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        }
        else if (screen == MenuScreen::Main)
        {
            m_mainMenu->Show(Rml::ModalFlag::None, Rml::FocusFlag::Auto);
        }
        else if (screen == MenuScreen::Pause)
        {
            m_pauseMenu->Show(Rml::ModalFlag::None, Rml::FocusFlag::Auto);
        }
        else if (screen == MenuScreen::FrameOver)
        {
            m_frameOver->Show(Rml::ModalFlag::None, Rml::FocusFlag::Auto);
        }
    }

    void ShellMenus::askConfirmation(const std::string& title, const std::string& detail,
                                     const std::string& action, std::function<void()> onConfirm)
    {
        m_confirmTitle = title;
        m_confirmDetail = detail;
        m_confirmAction = action;
        m_onConfirm = std::move(onConfirm);
        m_model.DirtyVariable("confirm_title");
        m_model.DirtyVariable("confirm_detail");
        m_model.DirtyVariable("confirm_action");
        // Modal: keyboard and gamepad focus stay inside the question.
        m_confirm->Show(Rml::ModalFlag::Modal, Rml::FocusFlag::Document);
        // Cancel is the safe default for keyboard and gamepad players.
        if (Rml::Element* cancel = m_confirm->GetElementById("confirmno"))
        {
            cancel->Focus(true);
        }
    }

    bool ShellMenus::confirmationOpen() const
    {
        return m_confirm->IsVisible();
    }

    void ShellMenus::cancelConfirmation()
    {
        m_onConfirm = nullptr;
        if (m_confirm->IsVisible())
        {
            m_confirm->Hide();
            // Give focus back to the menu underneath.
            if (m_shown == MenuScreen::Pause)
            {
                m_pauseMenu->Show(Rml::ModalFlag::None, Rml::FocusFlag::Auto);
            }
        }
    }

    void ShellMenus::setGamepadPrompts(bool gamepad)
    {
        m_gamepad = gamepad;
        m_model.DirtyVariable("gamepad");
    }

    void ShellMenus::setFrameResult(const FrameResultText& text)
    {
        m_frame = text;
        for (const char* name : {"frame_eyebrow", "frame_winner", "frame_detail", "frame_score", "frame_primary"})
        {
            m_model.DirtyVariable(name);
        }
    }

    void ShellMenus::showChoice(const std::string& player, const std::string& title, const std::string& detail,
                                const std::vector<std::string>& options, std::function<void(int)> onPick)
    {
        m_choicePlayer = player;
        m_choiceTitle = title;
        m_choiceDetail = detail;
        m_choiceCount = static_cast<int>(std::min<std::size_t>(options.size(), m_choiceOptions.size()));
        for (std::size_t i = 0; i < m_choiceOptions.size(); ++i)
        {
            m_choiceOptions[i] = (i < options.size()) ? options[i] : std::string();
        }
        m_onPick = std::move(onPick);
        for (const char* name : {"choice_player", "choice_title", "choice_detail", "choice_option0",
                                 "choice_option1", "choice_option2", "choice_count"})
        {
            m_model.DirtyVariable(name);
        }

        m_choice->Show(Rml::ModalFlag::Modal, Rml::FocusFlag::Document);
        // The first answer takes focus, so Enter or A picks it.
        if (Rml::Element* first = m_choice->GetElementById("choice0"))
        {
            first->Focus(true);
        }
    }

    bool ShellMenus::choiceOpen() const
    {
        return m_choice->IsVisible();
    }

    void ShellMenus::hideChoice()
    {
        m_onPick = nullptr;
        if (m_choice->IsVisible())
        {
            m_choice->Hide();
        }
    }
}
