#pragma once

#include "app/overlay_screens.h"
#include "ecs/entity.h"
#include "gameplay/match_session.h"
#include "platform/input.h"
#include "platform/timer.h"
#include "platform/window.h"
#include "render/camera_rig.h"
#include "render/scene_renderer.h"

#include <cstdint>
#include <memory>

namespace BilliardsSaloon
{
    enum class ApplicationShellState
    {
        MainMenu,
        Gameplay,
        PauseMenu
    };

    enum class MainMenuSelection
    {
        StartMatch,
        Fullscreen,
        Quit
    };

    enum class PauseMenuSelection
    {
        Resume,
        RestartRack,
        Fullscreen,
        ReturnToMainMenu
    };

    // Owns the window and main loop, routes input to the active screen,
    // and ties the match session, camera rig and renderer together.
    class Application
    {
    public:
        Application();
        int run();

    private:
        void processInput(float frameTimeSeconds);
        void processGlobalShortcuts();
        void processMainMenuInput();
        void processPauseMenuInput();
        void processGameplayInput(float frameTimeSeconds);
        void updateCursorCapture();

        // Mouse hover/click on the visible menu. Returns the clicked entry.
        [[nodiscard]] std::optional<std::size_t> processMenuMouse(std::size_t& selectedIndex);
        [[nodiscard]] bool menuConfirmPressed() const;

        void updateFixed(double deltaTimeSeconds);
        void updateCameraRig(double deltaTimeSeconds);
        [[nodiscard]] CameraRigContext buildGameplayCameraContext() const;

        [[nodiscard]] MenuScreenModel mainMenuModel() const;
        [[nodiscard]] MenuScreenModel pauseMenuModel() const;

        void render(double alpha);
        void updateWindowTitle(double frameTimeSeconds, std::uint32_t fixedStepsThisFrame);
        void refreshTitleSoon();

        void enterPauseMenu();
        void applyMainMenuSelection();
        void applyPauseMenuSelection();
        void setCameraViewMode(CameraViewMode mode);

        static constexpr double FIXED_TIME_STEP = 1.0 / 120.0;
        static constexpr double MAX_FRAME_TIME = 0.25;

        Window m_window;
        Timer m_timer;
        Input m_input;

        MatchSession m_session;
        Entity m_cameraEntity;
        std::unique_ptr<SceneRenderer> m_renderer;
        FrameView m_lastFrameView {};

        ApplicationShellState m_shellState {ApplicationShellState::MainMenu};
        MainMenuSelection m_mainMenuSelection {MainMenuSelection::StartMatch};
        PauseMenuSelection m_pauseMenuSelection {PauseMenuSelection::Resume};
        RenderQualityPreset m_renderQuality {RenderQualityPreset::Balanced};

        CameraRigState m_cameraRigState {};
        CameraRigInputAxes m_cameraInput {};

        double m_accumulator {0.0};

        bool m_showPerformanceStatsInTitle {true};
        double m_titleUpdateAccumulator {0.0};
        double m_titleUpdateFrameTimeSum {0.0};
        std::uint32_t m_titleUpdateFrameCount {0};
        std::uint32_t m_titleUpdateFixedStepCount {0};
    };
}
