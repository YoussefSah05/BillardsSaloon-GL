#pragma once

#include "ecs/entity.h"
#include "ecs/registry.h"
#include "gameplay/game_variant.h"
#include "gameplay/match_state.h"
#include "gameplay/shot_result.h"
#include "gameplay/shot_state.h"
#include "platform/timer.h"
#include "platform/window.h"
#include "render/mesh.h"
#include "render/shader.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace BilliardsSaloon
{
    enum class RenderQualityPreset
    {
        Low,
        Balanced,
        High
    };

    enum class ApplicationShellState
    {
        MainMenu,
        Gameplay,
        PauseMenu
    };

    enum class MainMenuSelection
    {
        StartMatch,
        Quit
    };

    enum class PauseMenuSelection
    {
        Resume,
        RestartRack,
        ReturnToMainMenu
    };

    class Application
    {
    public:
        Application();
        int run();

    private:
        void processPlatformInput();
        void updateFixed(double deltaTimeSeconds);
        void render(double alpha);
        void updateWindowTitle(double frameTimeSeconds, std::uint32_t fixedStepsThisFrame);
        void resetMatchToOpeningRack();
        void applyMainMenuSelection();
        void applyPauseMenuSelection();

        Entity findCueBall() const;
        void resetCueBall();
        bool fireCurrentShot();

        static constexpr double FIXED_TIME_STEP = 1.0 / 120.0;
        static constexpr double MAX_FRAME_TIME = 0.25;

        Window m_window;
        Timer m_timer;

        Registry m_registry;
        Entity m_cameraEntity;
        Entity m_cueBallEntity;
        std::vector<Entity> m_objectBallEntities;

        std::unique_ptr<Shader> m_basicShader;
        std::unique_ptr<Mesh> m_cubeMesh;
        std::unique_ptr<Mesh> m_planeMesh;
        std::unique_ptr<Mesh> m_sphereMesh;

        const GameVariantDefinition* m_variant {nullptr};
        MatchState m_matchState {};
        ShotState m_shotState {};
        ShotResult m_currentShotResult {};
        ApplicationShellState m_shellState {ApplicationShellState::MainMenu};
        MainMenuSelection m_mainMenuSelection {MainMenuSelection::StartMatch};
        PauseMenuSelection m_pauseMenuSelection {PauseMenuSelection::Resume};
        bool m_spaceWasDownLastFrame {false};
        bool m_escapeWasDownLastFrame {false};
        bool m_qualityToggleWasDownLastFrame {false};
        bool m_titleStatsToggleWasDownLastFrame {false};
        bool m_menuUpWasDownLastFrame {false};
        bool m_menuDownWasDownLastFrame {false};
        bool m_menuConfirmWasDownLastFrame {false};
        bool m_showPerformanceStatsInTitle {true};
        RenderQualityPreset m_renderQuality {RenderQualityPreset::Balanced};

        double m_accumulator {0.0};
        double m_simulationTime {0.0};
        double m_titleUpdateAccumulator {0.0};
        double m_titleUpdateFrameTimeSum {0.0};
        std::uint32_t m_titleUpdateFrameCount {0};
        std::uint32_t m_titleUpdateFixedStepCount {0};
        std::uint64_t m_fixedFrameIndex {0};
    };
}
