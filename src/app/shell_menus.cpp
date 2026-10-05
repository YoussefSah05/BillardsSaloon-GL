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
    }

    void ShellMenus::show(MenuScreen screen)
    {
        if (screen == m_shown)
        {
            return;
        }

        m_shown = screen;
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

    void ShellMenus::setFrameResult(const std::string& headline, const std::string& detail)
    {
        m_frameWinner = headline;
        m_frameDetail = detail;
        m_model.DirtyVariable("frame_winner");
        m_model.DirtyVariable("frame_detail");
    }
}
