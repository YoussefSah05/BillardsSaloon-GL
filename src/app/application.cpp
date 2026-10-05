#include "app/application.h"

#include "app/overlay_screens.h"
#include "app/saloon_scene.h"
#include "scene/components.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace BilliardsSaloon
{
    namespace
    {
        constexpr double TITLE_UPDATE_INTERVAL_SECONDS = 0.25;

        const char* shellStateLabel(ApplicationShellState shellState)
        {
            switch (shellState)
            {
                case ApplicationShellState::MainMenu:
                    return "Main Menu";

                case ApplicationShellState::Gameplay:
                    return "Gameplay";

                case ApplicationShellState::PauseMenu:
                    return "Paused";
            }

            return "Unknown";
        }

        const char* mainMenuSelectionLabel(MainMenuSelection selection)
        {
            switch (selection)
            {
                case MainMenuSelection::StartMatch:
                    return "Start Match";

                case MainMenuSelection::Quit:
                    return "Quit";
            }

            return "Unknown";
        }

        const char* pauseMenuSelectionLabel(PauseMenuSelection selection)
        {
            switch (selection)
            {
                case PauseMenuSelection::Resume:
                    return "Resume";

                case PauseMenuSelection::RestartRack:
                    return "Restart Rack";

                case PauseMenuSelection::ReturnToMainMenu:
                    return "Main Menu";
            }

            return "Unknown";
        }


        PauseMenuSelection previousPauseSelection(PauseMenuSelection selection)
        {
            switch (selection)
            {
                case PauseMenuSelection::Resume:
                    return PauseMenuSelection::ReturnToMainMenu;
                case PauseMenuSelection::RestartRack:
                    return PauseMenuSelection::Resume;
                case PauseMenuSelection::ReturnToMainMenu:
                    return PauseMenuSelection::RestartRack;
            }

            return PauseMenuSelection::Resume;
        }

        PauseMenuSelection nextPauseSelection(PauseMenuSelection selection)
        {
            switch (selection)
            {
                case PauseMenuSelection::Resume:
                    return PauseMenuSelection::RestartRack;
                case PauseMenuSelection::RestartRack:
                    return PauseMenuSelection::ReturnToMainMenu;
                case PauseMenuSelection::ReturnToMainMenu:
                    return PauseMenuSelection::Resume;
            }

            return PauseMenuSelection::Resume;
        }

        CameraViewMode nextCameraViewMode(CameraViewMode mode)
        {
            switch (mode)
            {
                case CameraViewMode::PlayerAim:
                    return CameraViewMode::TableOverview;
                case CameraViewMode::TableOverview:
                    return CameraViewMode::ShotFollow;
                case CameraViewMode::ShotFollow:
                    return CameraViewMode::FreeLook;
                case CameraViewMode::FreeLook:
                    return CameraViewMode::PlayerAim;
            }

            return CameraViewMode::PlayerAim;
        }

        float keyAxis(const Input& input, int negativeKey, int positiveKey)
        {
            return (input.isDown(positiveKey) ? 1.0f : 0.0f) - (input.isDown(negativeKey) ? 1.0f : 0.0f);
        }
    }

    Application::Application()
        : m_window(WindowDesc{})
        , m_session(eightBallVariant())
    {
        const GameVariantDefinition& variant = m_session.variant();

        m_renderer = std::make_unique<SceneRenderer>(
            variant.table.clothWidth,
            variant.table.clothDepth,
            variant.table.ballRadius
        );

        buildSaloonScene(m_session);
        m_cameraEntity = createMainCamera(m_session.registry());

        updateCameraRig(FIXED_TIME_STEP);
    }

    int Application::run()
    {
        m_timer.reset();

        while (!m_window.shouldClose())
        {
            m_window.pollEvents();

            const double frameTime = std::min(m_timer.tick(), MAX_FRAME_TIME);
            processInput(static_cast<float>(frameTime));

            m_accumulator += frameTime;
            std::uint32_t fixedStepsThisFrame = 0;

            while (m_accumulator >= FIXED_TIME_STEP)
            {
                updateFixed(FIXED_TIME_STEP);
                m_accumulator -= FIXED_TIME_STEP;
                ++fixedStepsThisFrame;
            }

            render(m_accumulator / FIXED_TIME_STEP);
            updateWindowTitle(frameTime, fixedStepsThisFrame);

            m_window.swapBuffers();
        }

        return 0;
    }

    void Application::refreshTitleSoon()
    {
        m_titleUpdateAccumulator = TITLE_UPDATE_INTERVAL_SECONDS;
    }

    void Application::processInput(float frameTimeSeconds)
    {
        m_input.update(m_window.nativeHandle());
        m_cameraInput = CameraRigInputAxes{};

        processGlobalShortcuts();

        switch (m_shellState)
        {
            case ApplicationShellState::MainMenu:
                processMainMenuInput();
                break;

            case ApplicationShellState::PauseMenu:
                processPauseMenuInput();
                break;

            case ApplicationShellState::Gameplay:
                processGameplayInput(frameTimeSeconds);
                break;
        }
    }

    void Application::processGlobalShortcuts()
    {
        if (m_input.wasPressed(GLFW_KEY_F2))
        {
            m_renderQuality = nextRenderQuality(m_renderQuality);
            refreshTitleSoon();
        }

        if (m_input.wasPressed(GLFW_KEY_F1))
        {
            m_showPerformanceStatsInTitle = !m_showPerformanceStatsInTitle;
            refreshTitleSoon();
        }
    }

    void Application::processMainMenuInput()
    {
        m_session.cancelHeldShot();

        if (m_input.wasPressed(GLFW_KEY_ESCAPE))
        {
            m_window.requestClose();
        }

        const bool up = m_input.wasPressed(GLFW_KEY_UP) || m_input.wasPressed(GLFW_KEY_W);
        const bool down = m_input.wasPressed(GLFW_KEY_DOWN) || m_input.wasPressed(GLFW_KEY_S);

        if (up || down)
        {
            m_mainMenuSelection =
                (m_mainMenuSelection == MainMenuSelection::StartMatch)
                ? MainMenuSelection::Quit
                : MainMenuSelection::StartMatch;
            refreshTitleSoon();
        }

        if (m_input.wasPressed(GLFW_KEY_ENTER) || m_input.wasPressed(GLFW_KEY_KP_ENTER))
        {
            applyMainMenuSelection();
        }
    }

    void Application::processPauseMenuInput()
    {
        m_session.cancelHeldShot();

        if (m_input.wasPressed(GLFW_KEY_ESCAPE))
        {
            m_shellState = ApplicationShellState::Gameplay;
            m_pauseMenuSelection = PauseMenuSelection::Resume;
            refreshTitleSoon();
            return;
        }

        if (m_input.wasPressed(GLFW_KEY_UP) || m_input.wasPressed(GLFW_KEY_W))
        {
            m_pauseMenuSelection = previousPauseSelection(m_pauseMenuSelection);
            refreshTitleSoon();
        }

        if (m_input.wasPressed(GLFW_KEY_DOWN) || m_input.wasPressed(GLFW_KEY_S))
        {
            m_pauseMenuSelection = nextPauseSelection(m_pauseMenuSelection);
            refreshTitleSoon();
        }

        if (m_input.wasPressed(GLFW_KEY_ENTER) || m_input.wasPressed(GLFW_KEY_KP_ENTER))
        {
            applyPauseMenuSelection();
        }
    }

    void Application::processGameplayInput(float frameTimeSeconds)
    {
        if (m_input.wasPressed(GLFW_KEY_ESCAPE))
        {
            enterPauseMenu();
            return;
        }

        if (m_input.wasPressed(GLFW_KEY_TAB))
        {
            setCameraViewMode(nextCameraViewMode(m_cameraRigState.mode));
        }

        if (m_input.wasPressed(GLFW_KEY_1))
        {
            setCameraViewMode(CameraViewMode::PlayerAim);
        }

        if (m_input.wasPressed(GLFW_KEY_2))
        {
            setCameraViewMode(CameraViewMode::TableOverview);
        }

        if (m_input.wasPressed(GLFW_KEY_3))
        {
            setCameraViewMode(CameraViewMode::ShotFollow);
        }

        if (m_input.wasPressed(GLFW_KEY_4))
        {
            setCameraViewMode(CameraViewMode::FreeLook);
        }

        if (m_cameraRigState.mode == CameraViewMode::FreeLook)
        {
            m_cameraInput.orbitYaw = keyAxis(m_input, GLFW_KEY_J, GLFW_KEY_L);
            m_cameraInput.orbitPitch = keyAxis(m_input, GLFW_KEY_K, GLFW_KEY_I);
            m_cameraInput.zoom = keyAxis(m_input, GLFW_KEY_O, GLFW_KEY_U);
        }

#if defined(BS_DEBUG)
        if (m_input.isDown(GLFW_KEY_R))
        {
            m_session.debugRespotCueBall();
        }
#endif

        m_session.applyShotControls(
            ShotControls{
                .aimAxis = keyAxis(m_input, GLFW_KEY_D, GLFW_KEY_A),
                .strikeRightAxis = keyAxis(m_input, GLFW_KEY_LEFT, GLFW_KEY_RIGHT),
                .strikeForwardAxis = keyAxis(m_input, GLFW_KEY_DOWN, GLFW_KEY_UP),
                .centerStrike = m_input.isDown(GLFW_KEY_C),
                .shootHeld = m_input.isDown(GLFW_KEY_SPACE)
            },
            frameTimeSeconds
        );
    }

    void Application::enterPauseMenu()
    {
        m_shellState = ApplicationShellState::PauseMenu;
        m_pauseMenuSelection = PauseMenuSelection::Resume;
        m_cameraInput = CameraRigInputAxes{};
        m_session.cancelHeldShot();
        refreshTitleSoon();
    }

    void Application::applyMainMenuSelection()
    {
        switch (m_mainMenuSelection)
        {
            case MainMenuSelection::StartMatch:
                m_session.resetRack();
                m_shellState = ApplicationShellState::Gameplay;
                break;

            case MainMenuSelection::Quit:
                m_window.requestClose();
                break;
        }

        refreshTitleSoon();
    }

    void Application::applyPauseMenuSelection()
    {
        switch (m_pauseMenuSelection)
        {
            case PauseMenuSelection::Resume:
                m_shellState = ApplicationShellState::Gameplay;
                break;

            case PauseMenuSelection::RestartRack:
                m_session.resetRack();
                m_shellState = ApplicationShellState::Gameplay;
                break;

            case PauseMenuSelection::ReturnToMainMenu:
                m_shellState = ApplicationShellState::MainMenu;
                break;
        }

        m_pauseMenuSelection = PauseMenuSelection::Resume;
        refreshTitleSoon();
    }

    void Application::setCameraViewMode(CameraViewMode mode)
    {
        m_cameraRigState.mode = mode;
        refreshTitleSoon();
    }

    void Application::updateFixed(double deltaTimeSeconds)
    {
        if (m_shellState == ApplicationShellState::Gameplay)
        {
            m_session.step(deltaTimeSeconds);
        }

        updateCameraRig(deltaTimeSeconds);
    }

    CameraRigContext Application::buildGameplayCameraContext() const
    {
        const float ballRadius = m_session.variant().table.ballRadius;

        CameraRigContext context;
        context.tableCenter = glm::vec3(0.0f, ballRadius, 0.0f);
        context.aimDirection = m_session.aimDirection();
        context.ballsInMotion = m_session.ballsInMotion();

        float fastestSpeedSquared = 0.0f;

        // The registry view API is non-const; this traversal only reads.
        const_cast<Registry&>(m_session.registry()).view<TransformComponent, BallComponent>().each(
            [&](Entity, TransformComponent& transform, BallComponent& ball)
            {
                if (ball.pocketed)
                {
                    return;
                }

                if (ball.isCueBall)
                {
                    context.cueBallPosition = transform.position;
                    context.cueBallAvailable = true;
                }

                glm::vec3 planarVelocity = ball.linearVelocity;
                planarVelocity.y = 0.0f;

                const float speedSquared = glm::dot(planarVelocity, planarVelocity);
                if (speedSquared > fastestSpeedSquared)
                {
                    fastestSpeedSquared = speedSquared;
                    context.trackedBallPosition = transform.position;
                    context.trackedBallVelocity = planarVelocity;
                    context.trackedBallAvailable = true;
                }
            }
        );

        if (!context.trackedBallAvailable && context.cueBallAvailable)
        {
            context.trackedBallPosition = context.cueBallPosition;
            context.trackedBallVelocity = glm::vec3(0.0f);
            context.trackedBallAvailable = true;
        }

        return context;
    }

    void Application::updateCameraRig(double deltaTimeSeconds)
    {
        TransformComponent* cameraTransform = m_session.registry().tryGet<TransformComponent>(m_cameraEntity);
        if (cameraTransform == nullptr)
        {
            return;
        }

        CameraPose targetPose;

        if (m_shellState == ApplicationShellState::Gameplay)
        {
            if (m_cameraRigState.mode == CameraViewMode::FreeLook)
            {
                applyCameraRigInput(m_cameraRigState, m_cameraInput, static_cast<float>(deltaTimeSeconds));
            }

            targetPose = desiredCameraPose(m_cameraRigState, buildGameplayCameraContext());
        }
        else
        {
            const glm::vec3 cueSpot = m_session.cueBallStartPosition();

            CameraRigState menuRigState;
            menuRigState.mode = CameraViewMode::TableOverview;
            targetPose = desiredCameraPose(
                menuRigState,
                CameraRigContext{
                    .tableCenter = glm::vec3(0.0f, cueSpot.y, 0.0f),
                    .cueBallPosition = cueSpot,
                    .trackedBallPosition = cueSpot,
                    .trackedBallVelocity = glm::vec3(0.0f),
                    .aimDirection = glm::vec3(0.0f, 0.0f, -1.0f),
                    .cueBallAvailable = true,
                    .trackedBallAvailable = true,
                    .ballsInMotion = false
                }
            );
        }

        const float blendRate =
            (m_shellState == ApplicationShellState::Gameplay) ? 10.0f : 7.0f;
        const float blendAlpha =
            (deltaTimeSeconds > 0.0)
            ? (1.0f - std::exp(-blendRate * static_cast<float>(deltaTimeSeconds)))
            : 1.0f;

        cameraTransform->syncPrevious();

        const CameraPose blendedPose = blendCameraPose(
            CameraPose{
                .position = cameraTransform->position,
                .rotation = cameraTransform->rotation
            },
            targetPose,
            blendAlpha,
            blendAlpha
        );

        cameraTransform->position = blendedPose.position;
        cameraTransform->rotation = blendedPose.rotation;
    }

    void Application::render(double alpha)
    {
        const MatchState& match = m_session.matchState();
        const ShotState& shot = m_session.shotState();
        const bool frameOver = match.flowPhase == MatchFlowPhase::FrameOver;
        const float renderAlpha = static_cast<float>(alpha);

        FrameSettings settings;
        settings.quality = m_renderQuality;
        settings.lights = saloonLightRig();
        settings.viewportWidth = m_window.width();
        settings.viewportHeight = m_window.height();

        switch (m_shellState)
        {
            case ApplicationShellState::MainMenu:
                settings.clearColor = glm::vec3(0.025f, 0.020f, 0.022f);
                break;

            case ApplicationShellState::PauseMenu:
                settings.clearColor = glm::vec3(0.022f, 0.020f, 0.024f);
                break;

            case ApplicationShellState::Gameplay:
                settings.clearColor = frameOver
                    ? glm::vec3(0.03f, 0.025f, 0.03f)
                    : glm::vec3(0.035f, 0.025f, 0.02f);
                break;
        }

        FrameView view;
        if (!m_renderer->beginFrame(m_session.registry(), m_cameraEntity, renderAlpha, settings, view))
        {
            return;
        }

        Registry& registry = m_session.registry();
        const Entity cueBall = m_session.cueBallEntity();
        const BallComponent& cueBallState = registry.get<BallComponent>(cueBall);

        const bool aiming =
            (m_shellState == ApplicationShellState::Gameplay) &&
            m_session.acceptsShotInput();

        BallHighlight highlight;
        if (aiming)
        {
            const float glow = 0.05f + 0.18f * shot.charge01;
            highlight.ball = cueBall;
            highlight.emission = glm::vec3(glow, glow, glow * 0.82f);
        }

        m_renderer->drawWorld(registry, renderAlpha, highlight);

        if (aiming && !cueBallState.pocketed)
        {
            const glm::vec3 cueBallPosition =
                interpolateTransform(registry.get<TransformComponent>(cueBall), renderAlpha).position;

            m_renderer->drawAimGuide(
                cueBallPosition,
                cueBallState.radius,
                m_session.aimDirection(),
                shot.aimAngleRadians,
                shot.charge01,
                shot.strikeRight01,
                shot.strikeForward01
            );
        }

        switch (m_shellState)
        {
            case ApplicationShellState::Gameplay:
            {
                const int activePlayer = std::clamp(match.activePlayerIndex, 0, 1);

                drawGameplayHud(*m_renderer, view, GameplayHudModel{
                    .disciplineName = m_session.variant().displayName,
                    .activePlayerIndex = activePlayer,
                    .activePlayerGroup = match.players[activePlayer].targetGroup,
                    .shotPhase = shot.phase,
                    .flowPhase = match.flowPhase,
                    .foulCommitted = match.foulCommittedThisTurn,
                    .ballInHand = match.ballInHand,
                    .winnerPlayerIndex = frameOver ? match.winnerPlayerIndex : -1,
                    .charge01 = shot.charge01,
                    .quality = m_renderQuality,
                    .cameraMode = m_cameraRigState.mode
                });
                break;
            }

            case ApplicationShellState::MainMenu:
                drawMenuScreen(*m_renderer, view, MenuScreenModel{
                    .title = "MAIN MENU",
                    .entries = {
                        mainMenuSelectionLabel(MainMenuSelection::StartMatch),
                        mainMenuSelectionLabel(MainMenuSelection::Quit)
                    },
                    .selectedIndex = static_cast<std::size_t>(m_mainMenuSelection),
                    .hints = {"UP/DOWN SELECT", "ENTER CONFIRM", "ESC QUIT"}
                });
                break;

            case ApplicationShellState::PauseMenu:
                drawMenuScreen(*m_renderer, view, MenuScreenModel{
                    .title = "PAUSED",
                    .entries = {
                        pauseMenuSelectionLabel(PauseMenuSelection::Resume),
                        pauseMenuSelectionLabel(PauseMenuSelection::RestartRack),
                        pauseMenuSelectionLabel(PauseMenuSelection::ReturnToMainMenu)
                    },
                    .selectedIndex = static_cast<std::size_t>(m_pauseMenuSelection),
                    .hints = {"UP/DOWN SELECT", "ENTER CONFIRM", "ESC RESUME"},
                    .titleOffset = 0.40f,
                    .titleBoxWidth = 0.44f,
                    .titleBoxEmission = glm::vec3(0.025f, 0.02f, 0.02f),
                    .firstEntryOffset = 0.20f,
                    .entrySpacing = 0.18f
                });
                break;
        }
    }

    void Application::updateWindowTitle(double frameTimeSeconds, std::uint32_t fixedStepsThisFrame)
    {
        m_titleUpdateAccumulator += frameTimeSeconds;
        m_titleUpdateFrameTimeSum += frameTimeSeconds;
        ++m_titleUpdateFrameCount;
        m_titleUpdateFixedStepCount += fixedStepsThisFrame;

        if (m_titleUpdateAccumulator < TITLE_UPDATE_INTERVAL_SECONDS)
        {
            return;
        }

        std::string title = "Billiards Saloon";
        title += " | ";
        title += shellStateLabel(m_shellState);
        title += " | Q:";
        title += renderQualityLabel(m_renderQuality);
        title += " | Cam:";
        title += cameraViewModeLabel(m_cameraRigState.mode);

        if (m_shellState == ApplicationShellState::MainMenu)
        {
            title += " | Up/Down Select | Enter Confirm | ";
            title += (m_mainMenuSelection == MainMenuSelection::StartMatch) ? "[" : "";
            title += mainMenuSelectionLabel(MainMenuSelection::StartMatch);
            title += (m_mainMenuSelection == MainMenuSelection::StartMatch) ? "]" : "";
            title += " ";
            title += (m_mainMenuSelection == MainMenuSelection::Quit) ? "[" : "";
            title += mainMenuSelectionLabel(MainMenuSelection::Quit);
            title += (m_mainMenuSelection == MainMenuSelection::Quit) ? "]" : "";
        }
        else if (m_shellState == ApplicationShellState::PauseMenu)
        {
            title += " | Up/Down Select | Enter Confirm | ";
            title += (m_pauseMenuSelection == PauseMenuSelection::Resume) ? "[" : "";
            title += pauseMenuSelectionLabel(PauseMenuSelection::Resume);
            title += (m_pauseMenuSelection == PauseMenuSelection::Resume) ? "]" : "";
            title += " ";
            title += (m_pauseMenuSelection == PauseMenuSelection::RestartRack) ? "[" : "";
            title += pauseMenuSelectionLabel(PauseMenuSelection::RestartRack);
            title += (m_pauseMenuSelection == PauseMenuSelection::RestartRack) ? "]" : "";
            title += " ";
            title += (m_pauseMenuSelection == PauseMenuSelection::ReturnToMainMenu) ? "[" : "";
            title += pauseMenuSelectionLabel(PauseMenuSelection::ReturnToMainMenu);
            title += (m_pauseMenuSelection == PauseMenuSelection::ReturnToMainMenu) ? "]" : "";
        }
        else
        {
            title += " | Tab Cycle Camera | 1 Aim | 2 Overview | 3 Follow | 4 Free";
        }

        if (m_showPerformanceStatsInTitle && (m_titleUpdateFrameTimeSum > 0.0))
        {
            const double averageFrameSeconds =
                m_titleUpdateFrameTimeSum / static_cast<double>(m_titleUpdateFrameCount);
            const double fps = static_cast<double>(m_titleUpdateFrameCount) / m_titleUpdateFrameTimeSum;
            const double fixedHz =
                static_cast<double>(m_titleUpdateFixedStepCount) / m_titleUpdateFrameTimeSum;
            const int roundedFps = static_cast<int>(std::round(fps));
            const int roundedFrameMs = static_cast<int>(std::round(averageFrameSeconds * 1000.0));
            const int roundedFixedHz = static_cast<int>(std::round(fixedHz));

            title += " | ";
            title += std::to_string(roundedFps);
            title += " FPS";
            title += " | ";
            title += std::to_string(roundedFrameMs);
            title += " ms";
            title += " | fixed ";
            title += std::to_string(roundedFixedHz);
            title += " Hz";
        }

        title += " | F1 title stats | F2 quality";

        m_window.setTitle(title);

        m_titleUpdateAccumulator = 0.0;
        m_titleUpdateFrameTimeSum = 0.0;
        m_titleUpdateFrameCount = 0;
        m_titleUpdateFixedStepCount = 0;
    }
}
