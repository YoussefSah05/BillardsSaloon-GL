#pragma once

#include "app/hud_screen.h"
#include "app/shell_menus.h"
#include "core/launch_options.h"
#include "ecs/entity.h"
#include "gameplay/match_session.h"
#include "platform/input.h"
#include "platform/timer.h"
#include "platform/window.h"
#include "render/camera_rig.h"
#include "render/scene_renderer.h"
#include "ui/ui_system.h"

#include <cstdint>
#include <memory>

namespace BilliardsSaloon
{
    enum class ApplicationShellState
    {
        MainMenu,
        Gameplay,
        PauseMenu,
        FrameOver
    };

    // Owns the window and main loop, routes input to the active screen,
    // and ties the match session, camera rig, renderer and UI together.
    class Application
    {
    public:
        explicit Application(const LaunchOptions& options = {});
        int run();

    private:
        void processInput(float frameTimeSeconds);
        void processGlobalShortcuts();
        void processGameplayInput(float frameTimeSeconds);
        void updateCursorCapture();

        void setShellState(ApplicationShellState state);
        void toggleFullscreen();

        void updateFixed(double deltaTimeSeconds);
        void updateCameraRig(double deltaTimeSeconds);
        [[nodiscard]] CameraRigContext buildGameplayCameraContext() const;

        void updateHud(float frameTimeSeconds);
        [[nodiscard]] HudSnapshot buildHudSnapshot() const;

        void render(double alpha);
        void updateWindowTitle(double frameTimeSeconds, std::uint32_t fixedStepsThisFrame);
        void refreshTitleSoon();

        void setCameraViewMode(CameraViewMode mode);

        // Development capture (--capture): saves the frame, then closes.
        void captureIfDue();

        static constexpr double FIXED_TIME_STEP = 1.0 / 120.0;
        static constexpr double MAX_FRAME_TIME = 0.25;

        // Declaration order is construction order: the window (and its GL
        // context) first; the menus last, because they hold UI documents.
        LaunchOptions m_options;
        std::uint64_t m_frameCount {0};

        Window m_window;
        Timer m_timer;
        Input m_input;

        MatchSession m_session;
        Entity m_cameraEntity;
        std::unique_ptr<SceneRenderer> m_renderer;
        std::unique_ptr<UiSystem> m_ui;
        std::unique_ptr<HudScreen> m_hud;
        std::unique_ptr<ShellMenus> m_menus;

        ApplicationShellState m_shellState {ApplicationShellState::MainMenu};
        RenderQualityPreset m_renderQuality {RenderQualityPreset::Balanced};

        CameraRigState m_cameraRigState {};
        CameraRigInputAxes m_cameraInput {};

        // A click or key that started the match (or resumed it) may still be
        // held; ignore shot input until it is released.
        bool m_waitForShotRelease {false};

        std::uint32_t m_announcedShots {0};
        float m_frameOverDelay {-1.0f};      // seconds until the frame-over card
        ShotPhase m_previousShotPhase {ShotPhase::Aiming};
        float m_chargeBeforeShot {0.0f};

        double m_accumulator {0.0};

        bool m_showPerformanceStatsInTitle {true};
        double m_titleUpdateAccumulator {0.0};
        double m_titleUpdateFrameTimeSum {0.0};
        std::uint32_t m_titleUpdateFrameCount {0};
        std::uint32_t m_titleUpdateFixedStepCount {0};
    };
}
