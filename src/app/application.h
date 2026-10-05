#pragma once

#include "app/audio_engine.h"
#include "app/hud_screen.h"
#include "app/locker_screen.h"
#include "app/match_setup_screen.h"
#include "app/settings_screen.h"
#include "core/gamepad_math.h"
#include "core/settings.h"
#include "app/shell_menus.h"
#include "core/launch_options.h"
#include "ecs/entity.h"
#include "ai/planner.h"
#include "gameplay/director.h"
#include "gameplay/match_session.h"
#include "platform/input.h"
#include "platform/timer.h"
#include "platform/window.h"
#include "render/camera_rig.h"
#include "render/scene_renderer.h"
#include "ui/ui_system.h"

#include <cstdint>
#include <future>
#include <memory>
#include <optional>

namespace BilliardsSaloon
{
    enum class ApplicationShellState
    {
        Title,
        MainMenu,
        Gameplay,
        PauseMenu,
        FrameOver,
        Settings,
        MatchSetup,       // discipline and race length before a quick match
        Locker,           // equipment and hall, previewed on the table
        RefereeChoice     // a player answers the referee (re-rack, push-out reply...)
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
        void processPlacementInput(float frameTimeSeconds, const glm::vec2& mouse, bool fine);

        // Builds a new match for the setup (discipline, race) and its scene.
        void startMatch(const MatchSetup& setup);
        [[nodiscard]] MatchSetup savedMatchSetup() const;
        void openRefereeChoice();
        // Applies the chosen equipment to the scene, the cue and the lights.
        void applyEquipment();
        void setFrameResultText(const ShotOutcome& outcome, const HudSnapshot& snapshot);

        // D-pad / left stick / A / B drive the menus through the UI's own
        // keyboard navigation. Returns true when B (back) was pressed.
        bool processGamepadMenus(float frameTimeSeconds);
        void updateCursorCapture();

        void setShellState(ApplicationShellState state);
        [[nodiscard]] bool isInMatch() const;
        void toggleFullscreen();

        // Applies settings to the window, renderer, input and UI, then saves them.
        void applySettings(const GameSettings& settings);
        void openSettings();
        void closeSettings();

        void updateFixed(double deltaTimeSeconds);
        void updateCameraRig(double deltaTimeSeconds);
        [[nodiscard]] CameraRigContext buildGameplayCameraContext() const;

        void updateHud(float frameTimeSeconds);
        // Plays the sounds of the shot (or replay) on screen as playback passes them.
        void updateAudio();

        // The computer player: thinks on a worker thread, lines up, strikes.
        void updateAi(float frameTimeSeconds);
        void resetAi();
        [[nodiscard]] bool aiToAct() const;
        [[nodiscard]] HudSnapshot buildHudSnapshot() const;

        void render(double alpha);
        void drawCueAndGuides(const glm::vec3& cueBallPosition, float ballRadius);
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

        // Loaded before the window so it opens with the saved size mode and vsync.
        std::filesystem::path m_settingsFile;
        GameSettings m_settings;

        Window m_window;
        Timer m_timer;
        Input m_input;

        std::unique_ptr<MatchSession> m_session;
        Entity m_cameraEntity;
        std::unique_ptr<SceneRenderer> m_renderer;
        std::unique_ptr<AudioEngine> m_audio;
        std::unique_ptr<UiSystem> m_ui;
        std::unique_ptr<HudScreen> m_hud;
        std::unique_ptr<ShellMenus> m_menus;
        std::unique_ptr<SettingsScreen> m_settingsScreen;
        std::unique_ptr<MatchSetupScreen> m_matchSetup;
        std::unique_ptr<LockerScreen> m_locker;
        ApplicationShellState m_settingsReturnState {ApplicationShellState::MainMenu};

        ApplicationShellState m_shellState {ApplicationShellState::MainMenu};
        RenderQualityPreset m_renderQuality {RenderQualityPreset::Balanced};

        CameraRigState m_cameraRigState {};
        CameraRigInputAxes m_cameraInput {};

        // Menu camera: slow orbit time and the sideways lens shift that puts
        // the table beside the menu on the main hub.
        double m_menuOrbitSeconds {0.0};

        HoldRepeater m_navUp {0.35f, 0.11f};
        HoldRepeater m_navDown {0.35f, 0.11f};
        HoldRepeater m_navLeft {0.35f, 0.11f};
        HoldRepeater m_navRight {0.35f, 0.11f};
        bool m_showGamepadPrompts {false};
        float m_lensShift {0.0f};

        // A click or key that started the match (or resumed it) may still be
        // held; ignore shot input until it is released.
        bool m_waitForShotRelease {false};

        std::uint32_t m_announcedShots {0};
        float m_frameOverDelay {-1.0f};      // seconds until the frame-over card
        float m_choiceDelay {-1.0f};         // seconds until the referee's question

        // The cue as last drawn while aiming, and the follow-through after the strike.
        struct CuePose
        {
            glm::vec3 tip {0.0f};
            glm::vec3 butt {1.0f, 0.0f, 0.0f};   // direction from the tip to the butt
            float pullback {0.0f};
        };
        CuePose m_cuePose;
        float m_followThroughSeconds {-1.0f};

        // The director's plan for the shot or replay on screen.
        std::vector<DirectorCut> m_directorCuts;
        std::uint32_t m_plannedPlayback {0};
        std::ptrdiff_t m_directorCut {-1};

        // A frame-winning pot is replayed in slow motion before the result card.
        bool m_replayBeforeCard {false};
        bool m_cardAfterReplay {false};

        // The computer opponent (seat 1), if any.
        struct AiPlan
        {
            bool choice {false};
            Rules::Option option {Rules::Option::Play};
            bool place {false};
            glm::vec2 spot {0.0f};
            Ai::AiShot shot;
        };
        enum class AiStage { Idle, Thinking, Aiming };
        int m_aiSeat {-1};
        Ai::AiProfile m_aiProfile;
        AiStage m_aiStage {AiStage::Idle};
        std::future<AiPlan> m_aiFuture;
        float m_aiElapsed {0.0f};
        AiPlan m_aiPlan;
        ShotInput m_aiFrom;
        std::mt19937 m_aiRandom {2026};

        // Shot sounds for the playback on screen.
        std::vector<Audio::SoundCue> m_soundCues;
        std::size_t m_nextSoundCue {0};
        std::uint32_t m_soundPlayback {0};
        double m_lastPlaybackSeconds {0.0};
        std::optional<CameraViewMode> m_cameraBeforePlacing;
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
