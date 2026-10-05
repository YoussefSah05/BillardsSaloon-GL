#include "app/application.h"

#include "app/saloon_scene.h"
#include "render/screenshot.h"
#include "scene/components.h"

#include <GLFW/glfw3.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <optional>
#include <random>
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
                case ApplicationShellState::Title:
                    return "Title";

                case ApplicationShellState::MainMenu:
                    return "Main Menu";

                case ApplicationShellState::Gameplay:
                    return "Gameplay";

                case ApplicationShellState::PauseMenu:
                    return "Paused";

                case ApplicationShellState::FrameOver:
                    return "Frame over";

                case ApplicationShellState::Settings:
                    return "Settings";

                case ApplicationShellState::MatchSetup:
                    return "Match setup";

                case ApplicationShellState::RefereeChoice:
                    return "Referee";
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
        constexpr std::uint32_t PROMPT_SHOTS = 4;

        // Ball in hand: how fast the cue ball moves.
        constexpr float PLACE_METERS_PER_POINT = 0.0012f;
        constexpr float PLACE_METERS_PER_SECOND = 0.6f;
        constexpr float PLACE_FINE_SCALE = 0.25f;

        // Let the referee's banner play before a question or the frame card.
        constexpr float CHOICE_DELAY_SECONDS = 1.4f;

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

    namespace
    {
        std::filesystem::path settingsFilePath()
        {
            const std::filesystem::path directory = userDataDirectory();
            return directory.empty() ? std::filesystem::path{} : directory / "settings.json";
        }

        GameSettings loadStartupSettings(const std::filesystem::path& file, const LaunchOptions& options)
        {
            GameSettings settings;
            if (!file.empty())
            {
                std::string warning;
                settings = loadSettings(file, &warning);
                if (!warning.empty())
                {
                    std::cerr << warning << '\n';
                }
            }
            if (options.fullscreen)
            {
                settings.fullscreen = true;
            }
            return settings;
        }

        RenderQualityPreset toRenderQuality(QualityLevel quality)
        {
            switch (quality)
            {
                case QualityLevel::Low:
                    return RenderQualityPreset::Low;
                case QualityLevel::Balanced:
                    return RenderQualityPreset::Balanced;
                case QualityLevel::High:
                    return RenderQualityPreset::High;
            }
            return RenderQualityPreset::Balanced;
        }

        QualityLevel toQualityLevel(RenderQualityPreset quality)
        {
            switch (quality)
            {
                case RenderQualityPreset::Low:
                    return QualityLevel::Low;
                case RenderQualityPreset::Balanced:
                    return QualityLevel::Balanced;
                case RenderQualityPreset::High:
                    return QualityLevel::High;
            }
            return QualityLevel::Balanced;
        }
    }

    Application::Application(const LaunchOptions& options)
        : m_options(options)
        , m_settingsFile(settingsFilePath())
        , m_settings(loadStartupSettings(m_settingsFile, options))
        , m_window(WindowDesc{.fullscreen = m_settings.fullscreen, .vsync = m_settings.vsync})
    {
        // Every discipline plays on the same table, so the renderer is built once.
        startMatch(savedMatchSetup());
        const GameVariantDefinition& variant = m_session->variant();

        m_renderer = std::make_unique<SceneRenderer>(
            variant.table.clothWidth,
            variant.table.clothDepth,
            variant.table.ballRadius
        );

        m_ui = std::make_unique<UiSystem>(m_window);
        m_hud = std::make_unique<HudScreen>(*m_ui, variant);
        m_menus = std::make_unique<ShellMenus>(*m_ui, ShellMenuActions{
            .startMatch = [this]() { setShellState(ApplicationShellState::MatchSetup); },
            .openSettings = [this]() { openSettings(); },
            .quit = [this]() { m_window.requestClose(); },
            .resume = [this]() { setShellState(ApplicationShellState::Gameplay); },
            .restartRack = [this]()
            {
                const auto restart = [this]()
                {
                    m_session->restartFrame();
                    m_hud->clearAnnouncements();
                    setShellState(ApplicationShellState::Gameplay);
                };
                // Mid-frame, ask first; from the frame-over card, just go.
                if (m_shellState == ApplicationShellState::PauseMenu)
                {
                    m_menus->askConfirmation("RESTART THE RACK?", "The current frame will be lost.", "RESTART", restart);
                }
                else
                {
                    restart();
                }
            },
            .returnToMainMenu = [this]()
            {
                const auto leave = [this]() { setShellState(ApplicationShellState::MainMenu); };
                if (m_shellState == ApplicationShellState::PauseMenu)
                {
                    m_menus->askConfirmation("LEAVE THE MATCH?", "The current frame will be lost.", "LEAVE", leave);
                }
                else
                {
                    leave();
                }
            },
            .frameContinue = [this]()
            {
                if (m_session->matchOver())
                {
                    m_session->startMatch();   // rematch: same discipline and race
                }
                else
                {
                    m_session->startNextFrame();
                }
                m_hud->clearAnnouncements();
                setShellState(ApplicationShellState::Gameplay);
            }
        });
        m_settingsScreen = std::make_unique<SettingsScreen>(*m_ui, m_settings, SettingsScreenActions{
            .apply = [this](const GameSettings& settings) { applySettings(settings); },
            .back = [this]() { closeSettings(); }
        });
        m_matchSetup = std::make_unique<MatchSetupScreen>(*m_ui, savedMatchSetup(), MatchSetupActions{
            .start = [this](const MatchSetup& setup)
            {
                GameSettings settings = m_settings;
                settings.matchGame = static_cast<int>(setup.game);
                settings.raceTo = setup.raceTo;
                settings.winnerBreaks = setup.breakOrder == Rules::BreakOrder::WinnerBreaks;
                applySettings(settings);

                startMatch(setup);
                setShellState(ApplicationShellState::Gameplay);
            },
            .back = [this]() { setShellState(ApplicationShellState::MainMenu); }
        });

        // Apply everything that is not already set by the window description.
        m_renderQuality = toRenderQuality(m_settings.quality);
        m_ui->setUiScale(m_settings.uiScale);
        m_ui->setReducedMotion(m_settings.reducedMotion);
        setShellState(
            (options.startScreen == StartScreen::Title) ? ApplicationShellState::Title : ApplicationShellState::MainMenu);

        if ((options.startScreen == StartScreen::Gameplay) || (options.startScreen == StartScreen::Pause))
        {
            setShellState(ApplicationShellState::Gameplay);
        }
        if (options.startScreen == StartScreen::Pause)
        {
            setShellState(ApplicationShellState::PauseMenu);
        }
        if (options.startScreen == StartScreen::Settings)
        {
            openSettings();
        }
        if (options.startScreen == StartScreen::MatchSetup)
        {
            setShellState(ApplicationShellState::MatchSetup);
        }

        if (options.scenario != DevScenario::None)
        {
            // Development: play a soft break the wrong way, so captures can show
            // ball in hand (9-ball foul) or the referee's question (8-ball).
            MatchSetup setup = savedMatchSetup();
            setup.game = (options.scenario == DevScenario::Foul) ? GameDiscipline::NineBall
                : (options.scenario == DevScenario::Call) ? GameDiscipline::TenBall
                : GameDiscipline::EightBall;
            startMatch(setup);
            if (options.scenario == DevScenario::Call)
            {
                // Pot the 1 off a layout "break" (legal, shooter stays), then aim at the 2.
                m_session->setLayout(glm::vec2(0.9165f, 0.2815f),
                    {{1, glm::vec2(1.1286f, 0.4936f)}, {2, glm::vec2(0.2f, -0.3f)}, {7, glm::vec2(-0.5f, 0.2f)}, {10, glm::vec2(-0.2f, 0.35f)}});
            }

            ShotControls aim;
            // Aim angle a points along (sin a, 0, -cos a); the opening aim is pi/2.
            const auto angleTo = [](const glm::vec2& from, const glm::vec2& to)
            {
                return std::atan2(to.x - from.x, -(to.y - from.y));
            };
            aim.aimDeltaRadians = (options.scenario == DevScenario::Call)
                ? angleTo({0.9165f, 0.2815f}, {1.1286f, 0.4936f}) - m_session->shotState().aimAngleRadians
                : -3.14159265f;
            m_session->applyShotControls(aim, 0.0f);
            ShotControls hold;
            hold.shootHeld = true;
            for (int i = 0; i < 20; ++i)
            {
                m_session->applyShotControls(hold, 1.0f / 120.0f);
            }
            m_session->applyShotControls(ShotControls{}, 1.0f / 120.0f);
            for (int i = 0; (i < 120 * 60) && (m_session->shotState().phase == ShotPhase::BallsInMotion); ++i)
            {
                m_session->step(FIXED_TIME_STEP);
            }
            if (options.scenario == DevScenario::Call)
            {
                const glm::vec3 cue = m_session->registry().get<TransformComponent>(m_session->cueBallEntity()).position;
                const glm::vec3 two = m_session->registry().get<TransformComponent>(*m_session->ballEntity(2)).position;
                ShotControls toTwo;
                toTwo.aimDeltaRadians =
                    angleTo({cue.x, cue.z}, {two.x, two.z}) - m_session->shotState().aimAngleRadians;
                m_session->applyShotControls(toTwo, 0.0f);
            }
            setShellState(ApplicationShellState::Gameplay);
        }

        updateCameraRig(FIXED_TIME_STEP);
    }

    MatchSetup Application::savedMatchSetup() const
    {
        MatchSetup setup;
        setup.game = static_cast<GameDiscipline>(std::clamp(m_settings.matchGame, 0, MATCH_GAME_COUNT - 1));
        setup.raceTo = m_settings.raceTo;
        setup.breakOrder = m_settings.winnerBreaks ? Rules::BreakOrder::WinnerBreaks : Rules::BreakOrder::Alternate;
        return setup;
    }

    void Application::startMatch(const MatchSetup& setup)
    {
        MatchSettings settings;
        settings.raceTo = setup.raceTo;
        settings.breakOrder = setup.breakOrder;
        settings.seed = std::random_device{}();

        m_session = std::make_unique<MatchSession>(
            variantFor(setup.game), ShotInputTuning{},
            m_options.legacyPhysics ? PhysicsBackend::Legacy : PhysicsBackend::EventBased,
            settings);

        // The hall, the table's meshes and the camera live in the session's
        // registry, so a new session gets them again.
        buildSaloonScene(*m_session);
        m_cameraEntity = createMainCamera(m_session->registry());

        if (m_hud)
        {
            m_hud->setVariant(m_session->variant());
            m_hud->clearAnnouncements();
        }
        m_announcedShots = 0;
        m_choiceDelay = -1.0f;
        m_frameOverDelay = -1.0f;
        m_cameraBeforePlacing.reset();
        updateCameraRig(FIXED_TIME_STEP);
    }

    void Application::openRefereeChoice()
    {
        const Rules::Choice choice = m_session->pendingChoice();
        const int chooser = m_session->chooser();
        if ((choice == Rules::Choice::None) || (chooser < 0))
        {
            return;
        }

        const HudSnapshot snapshot = buildHudSnapshot();
        const std::string& player = snapshot.playerNames[static_cast<std::size_t>(chooser)];
        const std::string& other = snapshot.playerNames[static_cast<std::size_t>(Rules::otherPlayer(chooser))];

        std::string title;
        std::string detail;
        std::vector<std::string> labels;
        switch (choice)
        {
            case Rules::Choice::IllegalBreak:
                title = "ILLEGAL BREAK";
                detail = "Fewer than four balls reached a cushion and none dropped.";
                labels = {"PLAY FROM HERE", "RE-RACK AND BREAK", "RE-RACK, " + other + " BREAKS AGAIN"};
                break;
            case Rules::Choice::EightOnBreak:
                title = "8-BALL ON THE BREAK";
                detail = "The 8 has been spotted. Play on from here, or re-rack and break.";
                labels = {"PLAY FROM HERE", "RE-RACK AND BREAK"};
                break;
            case Rules::Choice::AfterPushOut:
                title = "PUSH OUT";
                detail = other + " pushed out. Take the shot from here, or hand it back.";
                labels = {"PLAY FROM HERE", "HAND IT BACK"};
                break;
            case Rules::Choice::AfterUncalledPot:
                title = "BALL DOWN, NOT AS CALLED";
                detail = other + " did not pot the called ball in the called pocket. Take the table, or hand it back.";
                labels = {"PLAY FROM HERE", "HAND IT BACK"};
                break;
            case Rules::Choice::None:
                return;
        }

        setShellState(ApplicationShellState::RefereeChoice);
        m_menus->showChoice(player, title, detail, labels, [this, choice](int index)
        {
            const std::vector<Rules::Option> options = Rules::optionsFor(choice);
            if ((index >= 0) && (static_cast<std::size_t>(index) < options.size()))
            {
                (void)m_session->choose(options[static_cast<std::size_t>(index)]);
            }
            setShellState(ApplicationShellState::Gameplay);
        });
    }

    void Application::setFrameResultText(const ShotOutcome& outcome, const HudSnapshot& snapshot)
    {
        const Rules::Verdict& verdict = outcome.verdict;
        const Rules::MatchScore& score = m_session->score();
        const std::string& winner = snapshot.playerNames[static_cast<std::size_t>(verdict.winner)];
        const std::string game = std::to_string(Rules::gameBall(snapshot.game));

        FrameResultText text;
        text.eyebrow = outcome.matchOver ? "MATCH OVER" : "FRAME " + std::to_string(m_session->frameNumber()) + " OVER";
        text.headline = winner + (outcome.matchOver ? " WINS THE MATCH" : " WINS THE FRAME");
        switch (verdict.end)
        {
            case Rules::FrameEnd::GameBallPotted:
                text.detail = "The " + game + " went down on a legal shot.";
                break;
            case Rules::FrameEnd::EightBallEarly:
                text.detail = "The 8 went down before the group was cleared.";
                break;
            case Rules::FrameEnd::EightBallOnFoul:
                text.detail = "The 8 went down on a foul.";
                break;
            case Rules::FrameEnd::EightBallWrongPocket:
                text.detail = "The 8 went into a pocket that was not called.";
                break;
            case Rules::FrameEnd::ThreeFouls:
                text.detail = "Three fouls in a row lose the frame.";
                break;
            case Rules::FrameEnd::None:
                break;
        }
        if (score.raceTo > 1)
        {
            text.score = std::to_string(score.frames[0]) + " – " + std::to_string(score.frames[1]) +
                         "  ·  RACE TO " + std::to_string(score.raceTo);
        }
        text.primary = outcome.matchOver ? "REMATCH" : "NEXT FRAME";
        m_menus->setFrameResult(text);
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
            updateHud(static_cast<float>(frameTime));

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

        const bool backPressed =
            (m_shellState != ApplicationShellState::Gameplay) && processGamepadMenus(frameTimeSeconds);
        const bool startPressed = m_input.gamepadPressed(GLFW_GAMEPAD_BUTTON_START);

        switch (m_shellState)
        {
            case ApplicationShellState::Title:
                if (m_input.anyPressed())
                {
                    setShellState(ApplicationShellState::MainMenu);
                }
                break;

            case ApplicationShellState::MainMenu:
                // Navigation and clicks are handled by the UI documents.
                if (m_input.wasPressed(GLFW_KEY_ESCAPE))
                {
                    m_window.requestClose();
                }
                break;

            case ApplicationShellState::PauseMenu:
                if (m_menus->confirmationOpen())
                {
                    if (m_input.wasPressed(GLFW_KEY_ESCAPE) || backPressed)
                    {
                        m_menus->cancelConfirmation();
                    }
                }
                else if (m_input.wasPressed(GLFW_KEY_ESCAPE) || backPressed || startPressed)
                {
                    setShellState(ApplicationShellState::Gameplay);
                }
                break;

            case ApplicationShellState::FrameOver:
                if (m_input.wasPressed(GLFW_KEY_ESCAPE))
                {
                    setShellState(ApplicationShellState::MainMenu);
                }
                break;

            case ApplicationShellState::Settings:
                if (m_input.wasPressed(GLFW_KEY_ESCAPE) || backPressed)
                {
                    closeSettings();
                }
                break;

            case ApplicationShellState::MatchSetup:
                if (m_input.wasPressed(GLFW_KEY_ESCAPE) || backPressed)
                {
                    setShellState(ApplicationShellState::MainMenu);
                }
                break;

            case ApplicationShellState::RefereeChoice:
                // The question stays until answered; pause is still allowed.
                if (m_input.wasPressed(GLFW_KEY_ESCAPE) || startPressed)
                {
                    setShellState(ApplicationShellState::PauseMenu);
                }
                break;

            case ApplicationShellState::Gameplay:
                processGameplayInput(frameTimeSeconds);
                break;
        }

        updateCursorCapture();

        // Prompts follow the device used last; tell every screen when it changes.
        const bool gamepadPrompts = m_input.lastDevice() == InputDevice::Gamepad;
        if (gamepadPrompts != m_showGamepadPrompts)
        {
            m_showGamepadPrompts = gamepadPrompts;
            m_menus->setGamepadPrompts(gamepadPrompts);
            m_settingsScreen->setGamepadPrompts(gamepadPrompts);
            m_matchSetup->setGamepadPrompts(gamepadPrompts);
        }
    }

    bool Application::processGamepadMenus(float frameTimeSeconds)
    {
        if (!m_input.hasGamepad())
        {
            return false;
        }

        const float stickX = m_input.gamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_X);
        const float stickY = m_input.gamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_Y);
        constexpr float STICK_AS_DPAD = 0.5f;

        const auto send = [this](int steps, Rml::Input::KeyIdentifier key)
        {
            for (int i = 0; i < steps; ++i)
            {
                m_ui->injectKey(key);
            }
        };

        send(m_navUp.update(m_input.gamepadDown(GLFW_GAMEPAD_BUTTON_DPAD_UP) || (stickY < -STICK_AS_DPAD), frameTimeSeconds), Rml::Input::KI_UP);
        send(m_navDown.update(m_input.gamepadDown(GLFW_GAMEPAD_BUTTON_DPAD_DOWN) || (stickY > STICK_AS_DPAD), frameTimeSeconds), Rml::Input::KI_DOWN);
        send(m_navLeft.update(m_input.gamepadDown(GLFW_GAMEPAD_BUTTON_DPAD_LEFT) || (stickX < -STICK_AS_DPAD), frameTimeSeconds), Rml::Input::KI_LEFT);
        send(m_navRight.update(m_input.gamepadDown(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT) || (stickX > STICK_AS_DPAD), frameTimeSeconds), Rml::Input::KI_RIGHT);

        if (m_input.gamepadPressed(GLFW_GAMEPAD_BUTTON_A))
        {
            m_ui->injectKey(Rml::Input::KI_RETURN);
        }

        return m_input.gamepadPressed(GLFW_GAMEPAD_BUTTON_B);
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
            GameSettings settings = m_settings;
            settings.quality = toQualityLevel(nextRenderQuality(m_renderQuality));
            applySettings(settings);
            m_settingsScreen->setSettings(m_settings);
        }

        if (m_input.wasPressed(GLFW_KEY_F1))
        {
            m_showPerformanceStatsInTitle = !m_showPerformanceStatsInTitle;
            refreshTitleSoon();
        }
    }

    void Application::processGameplayInput(float frameTimeSeconds)
    {
        if (m_input.wasPressed(GLFW_KEY_ESCAPE) || m_input.gamepadPressed(GLFW_GAMEPAD_BUTTON_START))
        {
            setShellState(ApplicationShellState::PauseMenu);
            return;
        }

        if (m_input.wasPressed(GLFW_KEY_TAB) || m_input.gamepadPressed(GLFW_GAMEPAD_BUTTON_Y))
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
        const glm::vec2 mouse =
            m_window.isCursorCaptured() ? m_input.mouseDelta() * m_settings.mouseSensitivity : glm::vec2(0.0f);
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
            m_session->debugRespotCueBall();
        }
#endif

        if (m_waitForShotRelease)
        {
            const bool anyShotInputHeld =
                leftHeld || rightHeld || m_input.isDown(GLFW_KEY_SPACE) || m_input.isDown(GLFW_KEY_ENTER) ||
                m_input.gamepadDown(GLFW_GAMEPAD_BUTTON_A);
            if (anyShotInputHeld)
            {
                return;
            }
            m_waitForShotRelease = false;
        }

        if (m_session->placingCueBall())
        {
            processPlacementInput(frameTimeSeconds, mouse, fineAim || m_input.gamepadDown(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER));
            return;
        }

        if (m_session->canPlaceCueBall() &&
            (m_input.wasPressed(GLFW_KEY_B) || m_input.gamepadPressed(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER)))
        {
            m_session->beginCueBallPlacement();
            return;
        }

        // Called shots: pocket and ball; push-out after the break.
        if (m_input.wasPressed(GLFW_KEY_Q) || m_input.gamepadPressed(GLFW_GAMEPAD_BUTTON_DPAD_LEFT))
        {
            m_session->cycleCalledPocket(-1);
        }
        if (m_input.wasPressed(GLFW_KEY_E) || m_input.gamepadPressed(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT))
        {
            m_session->cycleCalledPocket(1);
        }
        if (m_input.wasPressed(GLFW_KEY_Z) || m_input.gamepadPressed(GLFW_GAMEPAD_BUTTON_DPAD_UP))
        {
            m_session->cycleCalledBall(1);
        }
        if (m_session->pushOutAvailable() &&
            (m_input.wasPressed(GLFW_KEY_P) || m_input.gamepadPressed(GLFW_GAMEPAD_BUTTON_BACK)))
        {
            m_session->setPushOut(!m_session->pushOutDeclared());
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

        if (m_input.hasGamepad())
        {
            // Left stick aims (LB for fine aim); right stick moves the cue tip,
            // or orbits the camera in free look; hold A to charge, release to shoot.
            const bool padFine = m_input.gamepadDown(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER);
            const float rightX = m_input.gamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_X);
            const float rightY = m_input.gamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_Y);

            controls.aimAxis -= m_input.gamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_X) * (padFine ? FINE_AIM_SCALE : 1.0f);

            if (freeLook)
            {
                m_cameraInput.orbitYaw += rightX;
                m_cameraInput.orbitPitch -= rightY;
                m_cameraInput.zoom +=
                    m_input.gamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER) - m_input.gamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_TRIGGER);
            }
            else
            {
                controls.strikeRightAxis += rightX;
                controls.strikeForwardAxis -= rightY;
            }

            controls.centerStrike = controls.centerStrike || m_input.gamepadDown(GLFW_GAMEPAD_BUTTON_X);
            controls.shootHeld = controls.shootHeld || m_input.gamepadDown(GLFW_GAMEPAD_BUTTON_A);
        }

        m_session->applyShotControls(controls, frameTimeSeconds);
    }

    void Application::processPlacementInput(float frameTimeSeconds, const glm::vec2& mouse, bool fine)
    {
        // Top-down view while placing; the previous view returns afterwards.
        if (!m_cameraBeforePlacing)
        {
            m_cameraBeforePlacing = m_cameraRigState.mode;
            setCameraViewMode(CameraViewMode::TableOverview);
        }

        // Moves are relative to the camera, flattened onto the table.
        glm::vec3 forward(0.0f, 0.0f, -1.0f);
        if (const TransformComponent* camera = m_session->registry().tryGet<TransformComponent>(m_cameraEntity))
        {
            forward = camera->rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        }
        forward.y = 0.0f;
        forward = (glm::length(forward) > 1.0e-4f) ? glm::normalize(forward) : glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 right(-forward.z, 0.0f, forward.x);

        const float scale = fine ? PLACE_FINE_SCALE : 1.0f;
        float alongRight = mouse.x * PLACE_METERS_PER_POINT;
        float alongForward = -mouse.y * PLACE_METERS_PER_POINT;

        const float keyRight = keyAxis(m_input, GLFW_KEY_A, GLFW_KEY_D) + keyAxis(m_input, GLFW_KEY_LEFT, GLFW_KEY_RIGHT);
        const float keyForward = keyAxis(m_input, GLFW_KEY_S, GLFW_KEY_W) + keyAxis(m_input, GLFW_KEY_DOWN, GLFW_KEY_UP);
        alongRight += keyRight * PLACE_METERS_PER_SECOND * frameTimeSeconds;
        alongForward += keyForward * PLACE_METERS_PER_SECOND * frameTimeSeconds;

        if (m_input.hasGamepad())
        {
            alongRight += m_input.gamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_X) * PLACE_METERS_PER_SECOND * frameTimeSeconds;
            alongForward -= m_input.gamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_Y) * PLACE_METERS_PER_SECOND * frameTimeSeconds;
        }

        const glm::vec3 move = (right * alongRight + forward * alongForward) * scale;
        m_session->moveCueBall(glm::vec2(move.x, move.z));

        const bool confirm =
            m_input.wasMousePressed(GLFW_MOUSE_BUTTON_LEFT) || m_input.wasPressed(GLFW_KEY_SPACE) ||
            m_input.wasPressed(GLFW_KEY_ENTER) || m_input.gamepadPressed(GLFW_GAMEPAD_BUTTON_A);
        if (confirm && m_session->confirmCueBallPlacement())
        {
            // The button that placed the ball must come up before it can shoot.
            m_waitForShotRelease = true;
            setCameraViewMode(*m_cameraBeforePlacing);
            m_cameraBeforePlacing.reset();
        }
    }

    void Application::setShellState(ApplicationShellState state)
    {
        m_shellState = state;
        m_frameOverDelay = -1.0f;
        m_cameraInput = CameraRigInputAxes{};
        m_session->cancelHeldShot();

        switch (state)
        {
            case ApplicationShellState::Title:
                m_menus->show(MenuScreen::Title);
                break;

            case ApplicationShellState::MainMenu:
                m_menus->show(MenuScreen::Main);
                break;

            case ApplicationShellState::PauseMenu:
                m_menus->show(MenuScreen::Pause);
                break;

            case ApplicationShellState::FrameOver:
                m_menus->show(MenuScreen::FrameOver);
                break;

            case ApplicationShellState::Settings:
            case ApplicationShellState::MatchSetup:
            case ApplicationShellState::RefereeChoice:
                m_menus->show(MenuScreen::None);
                break;

            case ApplicationShellState::Gameplay:
                m_menus->show(MenuScreen::None);
                m_waitForShotRelease = true;
                break;
        }

        m_settingsScreen->setVisible(state == ApplicationShellState::Settings);
        m_matchSetup->setVisible(state == ApplicationShellState::MatchSetup);
        if (state != ApplicationShellState::RefereeChoice)
        {
            m_menus->hideChoice();
        }
        refreshTitleSoon();
    }

    void Application::toggleFullscreen()
    {
        GameSettings settings = m_settings;
        settings.fullscreen = !settings.fullscreen;
        applySettings(settings);
        m_settingsScreen->setSettings(m_settings);
    }

    void Application::applySettings(const GameSettings& settings)
    {
        const GameSettings previous = m_settings;
        m_settings = sanitized(settings);

        if (m_settings.fullscreen != m_window.isFullscreen())
        {
            m_window.setFullscreen(m_settings.fullscreen);
            m_input.discardNextMouseDelta();
        }
        if (m_settings.vsync != m_window.vsyncEnabled())
        {
            m_window.setVsync(m_settings.vsync);
        }
        m_renderQuality = toRenderQuality(m_settings.quality);
        if (m_settings.uiScale != previous.uiScale)
        {
            m_ui->setUiScale(m_settings.uiScale);
        }
        if (m_settings.reducedMotion != previous.reducedMotion)
        {
            m_ui->setReducedMotion(m_settings.reducedMotion);
        }

        if (!m_settingsFile.empty() && !saveSettings(m_settingsFile, m_settings))
        {
            std::cerr << "Could not save settings to " << m_settingsFile.string() << '\n';
        }
        refreshTitleSoon();
    }

    void Application::openSettings()
    {
        m_settingsReturnState = m_shellState;
        m_settingsScreen->setSettings(m_settings);
        setShellState(ApplicationShellState::Settings);
    }

    void Application::closeSettings()
    {
        setShellState(m_settingsReturnState);
    }

    void Application::setCameraViewMode(CameraViewMode mode)
    {
        m_cameraRigState.mode = mode;
        refreshTitleSoon();
    }

    bool Application::isInMatch() const
    {
        const ApplicationShellState state =
            (m_shellState == ApplicationShellState::Settings) ? m_settingsReturnState : m_shellState;
        return (state != ApplicationShellState::Title) && (state != ApplicationShellState::MainMenu) &&
               (state != ApplicationShellState::MatchSetup);
    }

    void Application::updateFixed(double deltaTimeSeconds)
    {
        if (m_shellState == ApplicationShellState::Gameplay)
        {
            m_session->step(deltaTimeSeconds);
        }
        else if (!isInMatch() && !m_settings.reducedMotion)
        {
            m_menuOrbitSeconds += deltaTimeSeconds;
        }

        updateCameraRig(deltaTimeSeconds);
    }

    CameraRigContext Application::buildGameplayCameraContext() const
    {
        const float ballRadius = m_session->variant().table.ballRadius;

        CameraRigContext context;
        context.tableCenter = glm::vec3(0.0f, ballRadius, 0.0f);
        context.aimDirection = m_session->aimDirection();
        context.ballsInMotion = m_session->ballsInMotion();

        float fastestSpeedSquared = 0.0f;

        // The registry view API is non-const; this traversal only reads.
        const_cast<Registry&>(m_session->registry()).view<TransformComponent, BallComponent>().each(
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
        TransformComponent* cameraTransform = m_session->registry().tryGet<TransformComponent>(m_cameraEntity);
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
        else if (isInMatch())
        {
            // Pause, frame over and in-match settings keep the match view.
            cameraTransform->syncPrevious();
            return;
        }
        else
        {
            // Title and hub: a slow crane move over the table. The title sits
            // higher and wider; the hub comes closer.
            const bool title = m_shellState == ApplicationShellState::Title;
            // Sway in an arc on the open side of the room rather than circling it.
            const float angle = 0.75f + 0.35f * std::sin(0.07f * static_cast<float>(m_menuOrbitSeconds));
            const float radius = title ? 3.6f : 3.0f;
            const float height = title ? 1.6f : 1.2f;
            const glm::vec3 target(0.0f, -0.05f, 0.0f);
            targetPose = lookAtPose(
                target + glm::vec3(std::cos(angle) * radius, height, std::sin(angle) * radius),
                target
            );
        }

        // On the hub the table sits to the right of the menu (wide windows only).
        const float wantedShift =
            ((m_shellState == ApplicationShellState::MainMenu) && (m_window.aspectRatio() > 1.3f)) ? 0.32f : 0.0f;
        m_lensShift += (wantedShift - m_lensShift) * (1.0f - std::exp(-4.0f * static_cast<float>(deltaTimeSeconds)));

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

    HudSnapshot Application::buildHudSnapshot() const
    {
        const Rules::FrameState& frame = m_session->frame();
        const ShotState& shot = m_session->shotState();
        const bool playing = m_shellState == ApplicationShellState::Gameplay;

        HudSnapshot snapshot;
        snapshot.game = m_session->variant().discipline;
        snapshot.activePlayer = std::clamp(frame.shooter, 0, 1);
        snapshot.groups = frame.groups;
        snapshot.frames = m_session->score().frames;
        snapshot.raceTo = m_session->score().raceTo;
        snapshot.fouls = frame.consecutiveFouls;
        snapshot.discipline = disciplineLabel(snapshot.game);
        snapshot.aiming = playing && m_session->acceptsShotInput();
        snapshot.placing = playing && m_session->placingCueBall();
        snapshot.placementValid = m_session->cueBallPlacementValid();
        snapshot.canPlace = playing && m_session->canPlaceCueBall();
        snapshot.behindHeadString = frame.ballInHand == Rules::BallInHand::BehindHeadString;
        snapshot.pushOutAvailable = m_session->pushOutAvailable();
        snapshot.pushOutDeclared = m_session->pushOutDeclared();
        snapshot.power01 = shot.charge01;
        snapshot.strikeRight01 = shot.strikeRight01;
        snapshot.strikeForward01 = shot.strikeForward01;
        snapshot.gamepadPrompts = m_showGamepadPrompts;
        // Control prompts teach the first few shots of a session, then step aside.
        snapshot.showPrompts = m_session->resolvedShotCount() < PROMPT_SHOTS;

        if (const std::optional<Rules::Call>& call = m_session->calledShot(); call && m_session->callRequired())
        {
            snapshot.calling = true;
            snapshot.callText = std::to_string(call->ball) + "  ·  " +
                pocketName(m_session->pocketPositions()[static_cast<std::size_t>(call->pocket)]);
        }

        std::string camera = cameraViewModeLabel(m_cameraRigState.mode);
        std::transform(camera.begin(), camera.end(), camera.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        snapshot.cameraLabel = camera + " CAM";

        const std::vector<int> onTable = m_session->objectBallsOnTable();
        if (snapshot.game != GameDiscipline::EightBall)
        {
            snapshot.tableBalls = onTable;
        }
        for (std::size_t player = 0; player < 2; ++player)
        {
            for (const int number : onTable)
            {
                if ((snapshot.groups[player] != Rules::Group::None) && (Rules::groupOf(number) == snapshot.groups[player]))
                {
                    snapshot.remainingBalls[player].push_back(number);
                }
            }
        }

        return snapshot;
    }

    void Application::updateHud(float frameTimeSeconds)
    {
        m_hud->setVisible(isInMatch());

        const HudSnapshot snapshot = buildHudSnapshot();

        // Remember the power of each shot as it is struck, for the meter's marker.
        const ShotState& shot = m_session->shotState();
        if (shot.phase == ShotPhase::Charging)
        {
            m_chargeBeforeShot = shot.charge01;
        }
        if ((shot.phase == ShotPhase::BallsInMotion) && (m_previousShotPhase != ShotPhase::BallsInMotion))
        {
            m_hud->markShotPower(m_chargeBeforeShot);
        }
        m_previousShotPhase = shot.phase;

        if (m_session->resolvedShotCount() != m_announcedShots)
        {
            m_announcedShots = m_session->resolvedShotCount();
            const ShotOutcome& outcome = m_session->lastOutcome();
            m_hud->announce(outcome, snapshot);

            if (outcome.verdict.frameOver && (outcome.verdict.winner >= 0))
            {
                setFrameResultText(outcome, snapshot);
                m_frameOverDelay = 1.8f;   // let the referee banner play first
            }
            else if (outcome.verdict.choice != Rules::Choice::None)
            {
                m_choiceDelay = CHOICE_DELAY_SECONDS;
            }
        }

        m_hud->update(snapshot, frameTimeSeconds);

        // The referee's question: after the banner, or straight away when the
        // game comes back from pause with a question still open.
        if ((m_shellState == ApplicationShellState::Gameplay) && (m_session->pendingChoice() != Rules::Choice::None))
        {
            m_choiceDelay -= frameTimeSeconds;
            if (m_choiceDelay <= 0.0f)
            {
                m_choiceDelay = -1.0f;
                openRefereeChoice();
            }
        }

        if ((m_frameOverDelay > 0.0f) && (m_shellState == ApplicationShellState::Gameplay))
        {
            m_frameOverDelay -= frameTimeSeconds;
            if (m_frameOverDelay <= 0.0f)
            {
                setShellState(ApplicationShellState::FrameOver);
            }
        }
    }

    void Application::render(double alpha)
    {
        const ShotState& shot = m_session->shotState();
        const bool frameOver = m_session->frameOver();
        const float renderAlpha = static_cast<float>(alpha);

        FrameSettings settings;
        settings.quality = m_renderQuality;
        settings.lights = saloonLightRig();
        settings.viewportWidth = m_window.width();
        settings.viewportHeight = m_window.height();
        settings.lensShiftX = m_lensShift;

        switch (m_shellState)
        {
            case ApplicationShellState::Title:
            case ApplicationShellState::MainMenu:
            case ApplicationShellState::MatchSetup:
                settings.clearColor = glm::vec3(0.025f, 0.020f, 0.022f);
                break;

            case ApplicationShellState::PauseMenu:
                settings.clearColor = glm::vec3(0.022f, 0.020f, 0.024f);
                break;

            case ApplicationShellState::Gameplay:
            case ApplicationShellState::FrameOver:
            case ApplicationShellState::Settings:
            case ApplicationShellState::RefereeChoice:
                settings.clearColor = frameOver
                    ? glm::vec3(0.03f, 0.025f, 0.03f)
                    : glm::vec3(0.035f, 0.025f, 0.02f);
                break;
        }

        FrameView view;
        if (!m_renderer->beginFrame(m_session->registry(), m_cameraEntity, renderAlpha, settings, view))
        {
            return;
        }

        Registry& registry = m_session->registry();
        const Entity cueBall = m_session->cueBallEntity();
        const BallComponent& cueBallState = registry.get<BallComponent>(cueBall);

        const bool aiming =
            (m_shellState == ApplicationShellState::Gameplay) &&
            m_session->acceptsShotInput();

        const bool placing = (m_shellState == ApplicationShellState::Gameplay) && m_session->placingCueBall();

        BallHighlight highlight;
        if (aiming)
        {
            const float glow = 0.05f + 0.18f * shot.charge01;
            highlight.ball = cueBall;
            highlight.emission = glm::vec3(glow, glow, glow * 0.82f);
        }
        else if (placing)
        {
            // Ball in hand: green where it may go, red where it overlaps a ball.
            highlight.ball = cueBall;
            highlight.emission = m_session->cueBallPlacementValid()
                ? glm::vec3(0.05f, 0.20f, 0.10f)
                : glm::vec3(0.30f, 0.03f, 0.02f);
        }

        m_renderer->drawWorld(registry, renderAlpha, highlight);

        const float ballRadius = cueBallState.radius;
        if (placing && (m_session->frame().ballInHand == Rules::BallInHand::BehindHeadString))
        {
            // The head string, the line the cue ball must stay behind.
            m_renderer->drawMarker(
                glm::vec3(m_session->headStringX(), 0.0008f, 0.0f),
                glm::vec3(0.004f, 0.0015f, m_session->variant().table.clothDepth),
                glm::vec3(0.90f, 0.86f, 0.78f), true);
        }

        if ((aiming || placing) && m_session->calledShot() && m_session->callRequired())
        {
            // The called pocket glows on the rail; a chip floats over the called ball.
            const Rules::Call& call = *m_session->calledShot();
            const glm::vec3 pocket = m_session->pocketPositions()[static_cast<std::size_t>(call.pocket)];
            const glm::vec3 gold(0.95f, 0.62f, 0.12f);
            m_renderer->drawMarker(glm::vec3(pocket.x, 0.008f, pocket.z), glm::vec3(0.07f, 0.002f, 0.07f), gold);
            if (const std::optional<Entity> ball = m_session->ballEntity(call.ball))
            {
                const glm::vec3 position = interpolateTransform(registry.get<TransformComponent>(*ball), renderAlpha).position;
                m_renderer->drawMarker(position + glm::vec3(0.0f, 2.2f * ballRadius, 0.0f),
                                       glm::vec3(0.4f * ballRadius, 0.12f * ballRadius, 0.4f * ballRadius), gold);
            }
        }

        if (aiming && !cueBallState.pocketed)
        {
            const glm::vec3 cueBallPosition =
                interpolateTransform(registry.get<TransformComponent>(cueBall), renderAlpha).position;

            m_renderer->drawAimGuide(
                cueBallPosition,
                cueBallState.radius,
                m_session->aimDirection(),
                shot.aimAngleRadians,
                shot.charge01,
                shot.strikeRight01,
                shot.strikeForward01
            );
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
