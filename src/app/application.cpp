#include "app/application.h"

#include "app/overlay_screens.h"
#include "app/saloon_scene.h"
#include "render/screenshot.h"
#include "scene/components.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
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

        // Mouse feel. Cursor deltas are in window coordinates (points), so
        // these behave the same on standard and HiDPI displays.
        constexpr float MOUSE_AIM_RADIANS_PER_POINT = 0.0025f;
        constexpr float FINE_AIM_SCALE = 0.15f;
        constexpr float STROKE_POWER_PER_POINT = 1.0f / 300.0f;
        constexpr float MOUSE_SPIN_PER_POINT = 1.0f / 250.0f;
        constexpr float MOUSE_ORBIT_RADIANS_PER_POINT = 0.005f;
        constexpr float WHEEL_ZOOM_METERS_PER_STEP = 0.15f;

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

    Application::Application(const LaunchOptions& options)
        : m_options(options)
        , m_window(WindowDesc{.fullscreen = options.fullscreen})
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

        m_ui = std::make_unique<UiSystem>(m_window);
        m_menus = std::make_unique<ShellMenus>(*m_ui, ShellMenuActions{
            .startMatch = [this]()
            {
                m_session.resetRack();
                setShellState(ApplicationShellState::Gameplay);
            },
            .toggleFullscreen = [this]() { toggleFullscreen(); },
            .quit = [this]() { m_window.requestClose(); },
            .resume = [this]() { setShellState(ApplicationShellState::Gameplay); },
            .restartRack = [this]()
            {
                m_session.resetRack();
                setShellState(ApplicationShellState::Gameplay);
            },
            .returnToMainMenu = [this]() { setShellState(ApplicationShellState::MainMenu); },
            .isFullscreen = [this]() { return m_window.isFullscreen(); }
        });
        m_menus->show(MenuScreen::Main);

        if (options.startScreen != StartScreen::MainMenu)
        {
            setShellState(ApplicationShellState::Gameplay);
        }
        if (options.startScreen == StartScreen::Pause)
        {
            setShellState(ApplicationShellState::PauseMenu);
        }

        updateCameraRig(FIXED_TIME_STEP);
    }

    void Application::captureIfDue()
    {
        if (m_options.capturePath.empty() ||
            (m_frameCount != static_cast<std::uint64_t>(m_options.captureAfterFrames)))
        {
            return;
        }

        if (saveBackBufferPng(m_options.capturePath, m_window.width(), m_window.height()))
        {
            std::cout << "Saved " << m_options.capturePath.string() << '\n';
        }
        else
        {
            std::cerr << "Could not save " << m_options.capturePath.string() << '\n';
        }
        m_window.requestClose();
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

            ++m_frameCount;
            captureIfDue();

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
        m_input.update(m_window);
        m_cameraInput = CameraRigInputAxes{};

        processGlobalShortcuts();

        switch (m_shellState)
        {
            case ApplicationShellState::MainMenu:
                // Navigation and clicks are handled by the UI documents.
                if (m_input.wasPressed(GLFW_KEY_ESCAPE))
                {
                    m_window.requestClose();
                }
                break;

            case ApplicationShellState::PauseMenu:
                if (m_input.wasPressed(GLFW_KEY_ESCAPE))
                {
                    setShellState(ApplicationShellState::Gameplay);
                }
                break;

            case ApplicationShellState::Gameplay:
                processGameplayInput(frameTimeSeconds);
                break;
        }

        updateCursorCapture();
    }

    void Application::updateCursorCapture()
    {
        // Capture the cursor only while playing, so aiming has unlimited travel
        // and menus (or other apps, after alt-tab) get a normal pointer.
        const bool wantCaptured =
            (m_shellState == ApplicationShellState::Gameplay) && m_window.isFocused();

        if (wantCaptured != m_window.isCursorCaptured())
        {
            m_window.setCursorCaptured(wantCaptured);
            m_input.discardNextMouseDelta();
        }

        m_ui->setPointerEnabled(!wantCaptured);
    }

    void Application::processGlobalShortcuts()
    {
        const bool altHeld = m_input.isDown(GLFW_KEY_LEFT_ALT) || m_input.isDown(GLFW_KEY_RIGHT_ALT);
        const bool commandControlHeld =
            (m_input.isDown(GLFW_KEY_LEFT_SUPER) || m_input.isDown(GLFW_KEY_RIGHT_SUPER)) &&
            (m_input.isDown(GLFW_KEY_LEFT_CONTROL) || m_input.isDown(GLFW_KEY_RIGHT_CONTROL));

        const bool toggleFullscreen =
            m_input.wasPressed(GLFW_KEY_F11) ||
            (altHeld && (m_input.wasPressed(GLFW_KEY_ENTER) || m_input.wasPressed(GLFW_KEY_KP_ENTER))) ||
            (commandControlHeld && m_input.wasPressed(GLFW_KEY_F));   // macOS convention

        if (toggleFullscreen)
        {
            this->toggleFullscreen();
        }

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

    void Application::processGameplayInput(float frameTimeSeconds)
    {
        if (m_input.wasPressed(GLFW_KEY_ESCAPE))
        {
            setShellState(ApplicationShellState::PauseMenu);
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

        const bool freeLook = (m_cameraRigState.mode == CameraViewMode::FreeLook);

        // Mouse motion only counts while the cursor is captured by the game.
        const glm::vec2 mouse = m_window.isCursorCaptured() ? m_input.mouseDelta() : glm::vec2(0.0f);
        const bool leftHeld = m_input.isMouseDown(GLFW_MOUSE_BUTTON_LEFT);
        const bool rightHeld = m_input.isMouseDown(GLFW_MOUSE_BUTTON_RIGHT);
        const bool fineAim = m_input.isDown(GLFW_KEY_LEFT_SHIFT) || m_input.isDown(GLFW_KEY_RIGHT_SHIFT);

        if (freeLook)
        {
            m_cameraInput.orbitYaw = keyAxis(m_input, GLFW_KEY_J, GLFW_KEY_L);
            m_cameraInput.orbitPitch = keyAxis(m_input, GLFW_KEY_K, GLFW_KEY_I);
            m_cameraInput.zoom = keyAxis(m_input, GLFW_KEY_O, GLFW_KEY_U);

            applyCameraRigDelta(
                m_cameraRigState,
                rightHeld ? mouse.x * MOUSE_ORBIT_RADIANS_PER_POINT : 0.0f,
                rightHeld ? mouse.y * MOUSE_ORBIT_RADIANS_PER_POINT : 0.0f,
                m_input.scrollDelta() * WHEEL_ZOOM_METERS_PER_STEP
            );
        }

#if defined(BS_DEBUG)
        if (m_input.isDown(GLFW_KEY_R))
        {
            m_session.debugRespotCueBall();
        }
#endif

        if (m_waitForShotRelease)
        {
            const bool anyShotInputHeld =
                leftHeld || rightHeld || m_input.isDown(GLFW_KEY_SPACE) || m_input.isDown(GLFW_KEY_ENTER);
            if (anyShotInputHeld)
            {
                return;
            }
            m_waitForShotRelease = false;
        }

        ShotControls controls;
        controls.aimAxis = keyAxis(m_input, GLFW_KEY_D, GLFW_KEY_A) * (fineAim ? FINE_AIM_SCALE : 1.0f);
        controls.strikeRightAxis = keyAxis(m_input, GLFW_KEY_LEFT, GLFW_KEY_RIGHT);
        controls.strikeForwardAxis = keyAxis(m_input, GLFW_KEY_DOWN, GLFW_KEY_UP);
        controls.centerStrike = m_input.isDown(GLFW_KEY_C);
        controls.shootHeld = m_input.isDown(GLFW_KEY_SPACE);

        if (leftHeld)
        {
            // Stroke: drag the mouse back (toward you) to add power. Aim is locked.
            controls.strokeHeld = true;
            controls.strokeDelta = mouse.y * STROKE_POWER_PER_POINT;
        }
        else if (rightHeld && !freeLook)
        {
            // Move the cue tip on the ball: right = right english, up = follow.
            controls.strikeDelta = glm::vec2(mouse.x, -mouse.y) * MOUSE_SPIN_PER_POINT;
        }
        else if (!rightHeld)
        {
            // Moving the mouse right turns the aim to the right.
            controls.aimDeltaRadians =
                -mouse.x * MOUSE_AIM_RADIANS_PER_POINT * (fineAim ? FINE_AIM_SCALE : 1.0f);
        }

        m_session.applyShotControls(controls, frameTimeSeconds);
    }

    void Application::setShellState(ApplicationShellState state)
    {
        m_shellState = state;
        m_cameraInput = CameraRigInputAxes{};
        m_session.cancelHeldShot();

        switch (state)
        {
            case ApplicationShellState::MainMenu:
                m_menus->show(MenuScreen::Main);
                break;

            case ApplicationShellState::PauseMenu:
                m_menus->show(MenuScreen::Pause);
                break;

            case ApplicationShellState::Gameplay:
                m_menus->show(MenuScreen::None);
                m_waitForShotRelease = true;
                break;
        }

        refreshTitleSoon();
    }

    void Application::toggleFullscreen()
    {
        m_window.toggleFullscreen();
        m_input.discardNextMouseDelta();
        m_menus->refresh();
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

        // The scorebug stays visible under the pause overlay.
        if (m_shellState != ApplicationShellState::MainMenu)
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
        }

        m_ui->update();
        m_ui->render();
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

        title += " | F1 stats | F2 quality | F11 fullscreen";

        m_window.setTitle(title);

        m_titleUpdateAccumulator = 0.0;
        m_titleUpdateFrameTimeSum = 0.0;
        m_titleUpdateFrameCount = 0;
        m_titleUpdateFixedStepCount = 0;
    }
}
