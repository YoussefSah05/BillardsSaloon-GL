#include "app/shell_menus.h"

#include "ui/ui_system.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>

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
        model.Bind("frame_winner", &m_frameWinner);
        model.Bind("frame_detail", &m_frameDetail);

        model.BindEventCallback("start_match", callback(m_actions.startMatch));
        model.BindEventCallback("open_settings", callback(m_actions.openSettings));
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

    void ShellMenus::setFrameResult(const std::string& headline, const std::string& detail)
    {
        m_frameWinner = headline;
        m_frameDetail = detail;
        m_model.DirtyVariable("frame_winner");
        m_model.DirtyVariable("frame_detail");
    }
}
