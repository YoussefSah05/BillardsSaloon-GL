#include "app/application.h"

#include "app/overlay_screens.h"
#include "app/saloon_scene.h"
#include "scene/components.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
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

        constexpr MainMenuSelection MAIN_MENU_ENTRIES[] = {
            MainMenuSelection::StartMatch,
            MainMenuSelection::Fullscreen,
            MainMenuSelection::Quit
        };

        constexpr PauseMenuSelection PAUSE_MENU_ENTRIES[] = {
            PauseMenuSelection::Resume,
            PauseMenuSelection::RestartRack,
            PauseMenuSelection::Fullscreen,
            PauseMenuSelection::ReturnToMainMenu
        };

        template <typename Selection, std::size_t Count>
        std::size_t indexOf(const Selection (&entries)[Count], Selection selection)
        {
            for (std::size_t i = 0; i < Count; ++i)
            {
                if (entries[i] == selection)
                {
                    return i;
                }
            }
            return 0;
        }

        // Moves a menu selection one step up (-1) or down (+1), wrapping around.
        template <typename Selection, std::size_t Count>
        Selection stepSelection(const Selection (&entries)[Count], Selection selection, int direction)
        {
            const std::size_t index = indexOf(entries, selection);
            const std::size_t next = (index + Count + static_cast<std::size_t>(direction + static_cast<int>(Count))) % Count;
            return entries[next];
        }

        const char* fullscreenLabel(bool fullscreen)
        {
            return fullscreen ? "Fullscreen: On" : "Fullscreen: Off";
        }

        const char* mainMenuLabel(MainMenuSelection selection, bool fullscreen)
        {
            switch (selection)
            {
                case MainMenuSelection::StartMatch:
                    return "Start Match";

                case MainMenuSelection::Fullscreen:
                    return fullscreenLabel(fullscreen);

                case MainMenuSelection::Quit:
                    return "Quit";
            }

            return "Unknown";
        }

        const char* pauseMenuLabel(PauseMenuSelection selection, bool fullscreen)
        {
            switch (selection)
            {
                case PauseMenuSelection::Resume:
                    return "Resume";

                case PauseMenuSelection::RestartRack:
                    return "Restart Rack";

                case PauseMenuSelection::Fullscreen:
                    return fullscreenLabel(fullscreen);

                case PauseMenuSelection::ReturnToMainMenu:
                    return "Main Menu";
            }

            return "Unknown";
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
        m_input.update(m_window);
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
            m_window.toggleFullscreen();
            m_input.discardNextMouseDelta();
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

    bool Application::menuConfirmPressed() const
    {
        // Alt+Enter toggles fullscreen and must not also confirm.
        const bool altHeld = m_input.isDown(GLFW_KEY_LEFT_ALT) || m_input.isDown(GLFW_KEY_RIGHT_ALT);
        return !altHeld && (m_input.wasPressed(GLFW_KEY_ENTER) || m_input.wasPressed(GLFW_KEY_KP_ENTER));
    }

    std::optional<std::size_t> Application::processMenuMouse(std::size_t& selectedIndex)
    {
        const MenuScreenModel model =
            (m_shellState == ApplicationShellState::MainMenu) ? mainMenuModel() : pauseMenuModel();

        const std::optional<std::size_t> hovered =
            menuEntryAt(m_lastFrameView, model, m_input.cursorNdc());

        if (!hovered)
        {
            return std::nullopt;
        }

        // Follow the pointer only when it moves, so keyboard selection is not
        // overridden by a resting cursor.
        const glm::vec2 delta = m_input.mouseDelta();
        if ((delta.x != 0.0f) || (delta.y != 0.0f))
        {
            if (selectedIndex != *hovered)
            {
                selectedIndex = *hovered;
                refreshTitleSoon();
            }
        }

        if (m_input.wasMousePressed(GLFW_MOUSE_BUTTON_LEFT))
        {
            selectedIndex = *hovered;
            return hovered;
        }

        return std::nullopt;
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
            m_mainMenuSelection = stepSelection(MAIN_MENU_ENTRIES, m_mainMenuSelection, up ? -1 : 1);
            refreshTitleSoon();
        }

        std::size_t selected = indexOf(MAIN_MENU_ENTRIES, m_mainMenuSelection);
        const bool clicked = processMenuMouse(selected).has_value();
        m_mainMenuSelection = MAIN_MENU_ENTRIES[selected];

        if (clicked || menuConfirmPressed())
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

        const bool up = m_input.wasPressed(GLFW_KEY_UP) || m_input.wasPressed(GLFW_KEY_W);
        const bool down = m_input.wasPressed(GLFW_KEY_DOWN) || m_input.wasPressed(GLFW_KEY_S);

        if (up || down)
        {
            m_pauseMenuSelection = stepSelection(PAUSE_MENU_ENTRIES, m_pauseMenuSelection, up ? -1 : 1);
            refreshTitleSoon();
        }

        std::size_t selected = indexOf(PAUSE_MENU_ENTRIES, m_pauseMenuSelection);
        const bool clicked = processMenuMouse(selected).has_value();
        m_pauseMenuSelection = PAUSE_MENU_ENTRIES[selected];

        if (clicked || menuConfirmPressed())
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

            case MainMenuSelection::Fullscreen:
                m_window.toggleFullscreen();
                m_input.discardNextMouseDelta();
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

            case PauseMenuSelection::Fullscreen:
                // Stay in the pause menu so the player sees the result.
                m_window.toggleFullscreen();
                m_input.discardNextMouseDelta();
                refreshTitleSoon();
                return;

            case PauseMenuSelection::ReturnToMainMenu:
                m_shellState = ApplicationShellState::MainMenu;
                break;
        }

        m_pauseMenuSelection = PauseMenuSelection::Resume;
        refreshTitleSoon();
    }

    MenuScreenModel Application::mainMenuModel() const
    {
        MenuScreenModel model;
        model.title = "MAIN MENU";
        for (const MainMenuSelection entry : MAIN_MENU_ENTRIES)
        {
            model.entries.emplace_back(mainMenuLabel(entry, m_window.isFullscreen()));
        }
        model.selectedIndex = indexOf(MAIN_MENU_ENTRIES, m_mainMenuSelection);
        model.hints = {"MOUSE OR UP/DOWN SELECT", "CLICK OR ENTER CONFIRM", "ESC QUIT"};
        model.titleOffset = 0.38f;
        model.firstEntryOffset = 0.18f;
        model.entrySpacing = 0.15f;
        return model;
    }

    MenuScreenModel Application::pauseMenuModel() const
    {
        MenuScreenModel model;
        model.title = "PAUSED";
        for (const PauseMenuSelection entry : PAUSE_MENU_ENTRIES)
        {
            model.entries.emplace_back(pauseMenuLabel(entry, m_window.isFullscreen()));
        }
        model.selectedIndex = indexOf(PAUSE_MENU_ENTRIES, m_pauseMenuSelection);
        model.hints = {"MOUSE OR UP/DOWN SELECT", "CLICK OR ENTER CONFIRM", "ESC RESUME"};
        model.titleOffset = 0.40f;
        model.titleBoxWidth = 0.44f;
        model.titleBoxEmission = glm::vec3(0.025f, 0.02f, 0.02f);
        model.firstEntryOffset = 0.21f;
        model.entrySpacing = 0.14f;
        return model;
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
        m_lastFrameView = view;

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
                drawMenuScreen(*m_renderer, view, mainMenuModel());
                break;

            case ApplicationShellState::PauseMenu:
                drawMenuScreen(*m_renderer, view, pauseMenuModel());
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
