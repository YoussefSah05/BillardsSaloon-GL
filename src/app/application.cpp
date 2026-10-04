#include "app/application.h"

#include "core/asset_paths.h"
#include "gameplay/turn_rules.h"
#include "physics/billiards_physics.h"
#include "render/camera.h"
#include "render/ui_overlay.h"
#include "scene/components.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace BilliardsSaloon
{
    namespace
    {
        constexpr int POINT_LIGHT_COUNT = 3;
        constexpr double TITLE_UPDATE_INTERVAL_SECONDS = 0.25;

        struct PointLightRig
        {
            std::array<glm::vec3, POINT_LIGHT_COUNT> positions {};
            std::array<glm::vec3, POINT_LIGHT_COUNT> colors {};
        };

        const char* renderQualityLabel(RenderQualityPreset quality)
        {
            switch (quality)
            {
                case RenderQualityPreset::Low:
                    return "Low";

                case RenderQualityPreset::Balanced:
                    return "Balanced";

                case RenderQualityPreset::High:
                    return "High";
            }

            return "Unknown";
        }

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

        const char* shotPhaseLabel(ShotPhase phase)
        {
            switch (phase)
            {
                case ShotPhase::Aiming:
                    return "AIMING";

                case ShotPhase::Charging:
                    return "CHARGING";

                case ShotPhase::BallsInMotion:
                    return "BALLS IN MOTION";
            }

            return "UNKNOWN";
        }

        const char* matchFlowPhaseLabel(MatchFlowPhase phase)
        {
            switch (phase)
            {
                case MatchFlowPhase::BreakShot:
                    return "BREAK SHOT";

                case MatchFlowPhase::TableOpen:
                    return "TABLE OPEN";

                case MatchFlowPhase::GroupsAssigned:
                    return "GROUPS ASSIGNED";

                case MatchFlowPhase::FrameOver:
                    return "FRAME OVER";
            }

            return "UNKNOWN";
        }

        const char* playerTargetGroupLabel(PlayerTargetGroup group)
        {
            switch (group)
            {
                case PlayerTargetGroup::None:
                    return "OPEN";

                case PlayerTargetGroup::Solids:
                    return "SOLIDS";

                case PlayerTargetGroup::Stripes:
                    return "STRIPES";
            }

            return "UNKNOWN";
        }

        Entity createSceneEntity(
            Registry& registry,
            const std::string& name,
            const TransformComponent& transform,
            const StaticMeshComponent& mesh,
            const MaterialComponent& material)
        {
            const Entity entity = registry.createEntity();
            registry.emplace<NameComponent>(entity, NameComponent{name});
            registry.emplace<TransformComponent>(entity, transform);
            registry.emplace<StaticMeshComponent>(entity, mesh);
            registry.emplace<MaterialComponent>(entity, material);
            return entity;
        }

        TransformComponent makeTransform(
            const glm::vec3& position,
            const glm::quat& rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            const glm::vec3& scale = glm::vec3(1.0f))
        {
            TransformComponent transform;
            transform.position = position;
            transform.previousPosition = position;
            transform.rotation = rotation;
            transform.previousRotation = rotation;
            transform.scale = scale;
            transform.previousScale = scale;
            return transform;
        }

        glm::vec3 aimDirectionFromAngle(float angleRadians)
        {
            return glm::normalize(glm::vec3(std::sin(angleRadians), 0.0f, -std::cos(angleRadians)));
        }

        PointLightRig buildSaloonLightRig()
        {
            PointLightRig rig;
            rig.positions = {
                glm::vec3(-0.82f, 1.58f, -0.04f),
                glm::vec3( 0.00f, 1.66f,  0.00f),
                glm::vec3( 0.82f, 1.58f, -0.04f)
            };
            rig.colors = {
                glm::vec3(4.4f, 3.1f, 1.7f),
                glm::vec3(5.2f, 3.7f, 2.0f),
                glm::vec3(4.4f, 3.1f, 1.7f)
            };
            return rig;
        }

        int ballVisualType(const BallComponent& ball)
        {
            if (ball.isCueBall)
            {
                return 1;
            }

            switch (ball.ruleTag)
            {
                case BallRuleTag::Stripe:
                    return 3;

                case BallRuleTag::Eight:
                    return 4;

                case BallRuleTag::Solid:
                case BallRuleTag::Numbered:
                case BallRuleTag::Red:
                case BallRuleTag::Color:
                case BallRuleTag::Cue:
                    return 2;
            }

            return 0;
        }

        std::vector<glm::vec3> buildTriangleRackPositions(
            std::size_t ballCount,
            float ballRadius,
            const glm::vec3& apexPosition,
            float spacingScale)
        {
            std::vector<glm::vec3> positions;
            positions.reserve(ballCount);

            const float diameter = 2.0f * ballRadius;
            const float rowSpacing = std::sqrt(3.0f) * ballRadius * spacingScale;

            std::size_t placed = 0;
            for (int row = 0; placed < ballCount; ++row)
            {
                const int rowCount = row + 1;
                const float z = apexPosition.z - static_cast<float>(row) * rowSpacing;

                for (int col = 0; (col < rowCount) && (placed < ballCount); ++col)
                {
                    const float x =
                        apexPosition.x +
                        (static_cast<float>(col) - 0.5f * static_cast<float>(row)) * diameter * spacingScale;

                    positions.push_back(glm::vec3(x, apexPosition.y, z));
                    ++placed;
                }
            }

            return positions;
        }

        std::vector<glm::vec3> buildDiamondRackPositions(
            std::size_t ballCount,
            float ballRadius,
            const glm::vec3& apexPosition,
            float spacingScale)
        {
            std::vector<glm::vec3> positions;
            positions.reserve(ballCount);

            const float diameter = 2.0f * ballRadius;
            const float rowSpacing = std::sqrt(3.0f) * ballRadius * spacingScale;

            const int rowCounts[5] = {1, 2, 3, 2, 1};

            std::size_t placed = 0;
            for (int row = 0; (row < 5) && (placed < ballCount); ++row)
            {
                const int rowCount = rowCounts[row];
                const float z = apexPosition.z - static_cast<float>(row) * rowSpacing;

                for (int col = 0; (col < rowCount) && (placed < ballCount); ++col)
                {
                    const float x =
                        apexPosition.x +
                        (static_cast<float>(col) - 0.5f * static_cast<float>(rowCount - 1)) * diameter * spacingScale;

                    positions.push_back(glm::vec3(x, apexPosition.y, z));
                    ++placed;
                }
            }

            return positions;
        }

        std::vector<glm::vec3> buildRackPositions(const GameVariantDefinition& variant)
        {
            switch (variant.rack.pattern)
            {
                case RackPattern::Triangle:
                    return buildTriangleRackPositions(
                        variant.objectBalls.size(),
                        variant.table.ballRadius,
                        variant.rack.apexPosition,
                        variant.rack.spacingScale
                    );

                case RackPattern::Diamond:
                    return buildDiamondRackPositions(
                        variant.objectBalls.size(),
                        variant.table.ballRadius,
                        variant.rack.apexPosition,
                        variant.rack.spacingScale
                    );
            }

            return {};
        }

        glm::vec2 clampStrikeOffset(float right01, float forward01, float maxRadius01)
        {
            glm::vec2 offset(right01, forward01);
            const float length = glm::length(offset);

            if (length > maxRadius01)
            {
                offset = (offset / length) * maxRadius01;
            }

            return offset;
        }
    }

    Application::Application()
        : m_window(WindowDesc{})
    {
        m_variant = &eightBallVariant();

        m_basicShader = std::make_unique<Shader>(
            resolveAssetPath("shaders/basic.vert").string(),
            resolveAssetPath("shaders/basic.frag").string()
        );

        m_cubeMesh = Mesh::createCube();
        m_planeMesh = Mesh::createPlane(m_variant->table.clothWidth, m_variant->table.clothDepth);
        m_sphereMesh = Mesh::createUVSphere(m_variant->table.ballRadius, 40U, 20U);

        const float halfWidth = 0.5f * m_variant->table.clothWidth;
        const float halfDepth = 0.5f * m_variant->table.clothDepth;
        const PointLightRig lightRig = buildSaloonLightRig();

        const MaterialComponent clothMaterial{
            .albedo = glm::vec3(0.07f, 0.36f, 0.16f),
            .specularStrength = 0.30f,
            .shininess = 24.0f,
            .surfaceType = MaterialSurfaceType::Cloth,
            .roughness = 0.58f,
            .reflectivity = 0.035f,
            .clearcoatStrength = 0.0f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };

        const MaterialComponent railMaterial{
            .albedo = glm::vec3(0.30f, 0.16f, 0.07f),
            .specularStrength = 0.62f,
            .shininess = 96.0f,
            .surfaceType = MaterialSurfaceType::Wood,
            .roughness = 0.26f,
            .reflectivity = 0.08f,
            .clearcoatStrength = 0.45f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };

        const MaterialComponent legMaterial{
            .albedo = glm::vec3(0.18f, 0.09f, 0.04f),
            .specularStrength = 0.36f,
            .shininess = 48.0f,
            .surfaceType = MaterialSurfaceType::Wood,
            .roughness = 0.42f,
            .reflectivity = 0.04f,
            .clearcoatStrength = 0.12f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };

        const MaterialComponent floorMaterial{
            .albedo = glm::vec3(0.26f, 0.16f, 0.09f),
            .specularStrength = 0.28f,
            .shininess = 32.0f,
            .surfaceType = MaterialSurfaceType::Wood,
            .roughness = 0.60f,
            .reflectivity = 0.035f,
            .clearcoatStrength = 0.08f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };

        const MaterialComponent wallMaterial{
            .albedo = glm::vec3(0.20f, 0.12f, 0.08f),
            .specularStrength = 0.08f,
            .shininess = 8.0f,
            .surfaceType = MaterialSurfaceType::Generic,
            .roughness = 0.92f,
            .reflectivity = 0.02f,
            .clearcoatStrength = 0.0f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };

        const MaterialComponent lampMaterial{
            .albedo = glm::vec3(0.98f, 0.86f, 0.62f),
            .specularStrength = 0.35f,
            .shininess = 96.0f,
            .surfaceType = MaterialSurfaceType::LampGlass,
            .roughness = 0.10f,
            .reflectivity = 0.10f,
            .clearcoatStrength = 0.18f,
            .emissionColor = glm::vec3(1.00f, 0.68f, 0.28f),
            .emissionIntensity = 1.80f
        };

        const Entity table = createSceneEntity(
            m_registry,
            "Table Cloth",
            makeTransform(glm::vec3(0.0f, 0.0f, 0.0f)),
            StaticMeshComponent{MeshPrimitive::Plane},
            clothMaterial
        );

        createSceneEntity(
            m_registry,
            "Saloon Floor",
            makeTransform(glm::vec3(0.0f, -0.46f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(4.8f, 1.0f, 4.8f)),
            StaticMeshComponent{MeshPrimitive::Plane},
            floorMaterial
        );

        createSceneEntity(
            m_registry,
            "Back Wall",
            makeTransform(
                glm::vec3(0.0f, 1.25f, -2.55f),
                glm::quat(glm::vec3(glm::radians(90.0f), 0.0f, 0.0f)),
                glm::vec3(5.6f, 1.0f, 2.6f)
            ),
            StaticMeshComponent{MeshPrimitive::Plane},
            wallMaterial
        );

        const float frameThickness = 0.16f;
        const float frameHeight = 0.12f;
        const float frameCenterY = -0.055f;

        createSceneEntity(
            m_registry,
            "North Rail",
            makeTransform(
                glm::vec3(0.0f, frameCenterY, -(halfDepth + 0.5f * frameThickness)),
                glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                glm::vec3(m_variant->table.clothWidth + 2.0f * frameThickness, frameHeight, frameThickness)
            ),
            StaticMeshComponent{MeshPrimitive::Cube},
            railMaterial
        );

        createSceneEntity(
            m_registry,
            "South Rail",
            makeTransform(
                glm::vec3(0.0f, frameCenterY, halfDepth + 0.5f * frameThickness),
                glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                glm::vec3(m_variant->table.clothWidth + 2.0f * frameThickness, frameHeight, frameThickness)
            ),
            StaticMeshComponent{MeshPrimitive::Cube},
            railMaterial
        );

        createSceneEntity(
            m_registry,
            "West Rail",
            makeTransform(
                glm::vec3(-(halfWidth + 0.5f * frameThickness), frameCenterY, 0.0f),
                glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                glm::vec3(frameThickness, frameHeight, m_variant->table.clothDepth)
            ),
            StaticMeshComponent{MeshPrimitive::Cube},
            railMaterial
        );

        createSceneEntity(
            m_registry,
            "East Rail",
            makeTransform(
                glm::vec3(halfWidth + 0.5f * frameThickness, frameCenterY, 0.0f),
                glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                glm::vec3(frameThickness, frameHeight, m_variant->table.clothDepth)
            ),
            StaticMeshComponent{MeshPrimitive::Cube},
            railMaterial
        );

        const float legOffsetX = halfWidth - 0.22f;
        const float legOffsetZ = halfDepth - 0.15f;
        const glm::vec3 legScale(0.16f, 0.84f, 0.16f);

        createSceneEntity(
            m_registry,
            "North West Leg",
            makeTransform(glm::vec3(-legOffsetX, -0.47f, -legOffsetZ), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), legScale),
            StaticMeshComponent{MeshPrimitive::Cube},
            legMaterial
        );

        createSceneEntity(
            m_registry,
            "North East Leg",
            makeTransform(glm::vec3(legOffsetX, -0.47f, -legOffsetZ), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), legScale),
            StaticMeshComponent{MeshPrimitive::Cube},
            legMaterial
        );

        createSceneEntity(
            m_registry,
            "South West Leg",
            makeTransform(glm::vec3(-legOffsetX, -0.47f, legOffsetZ), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), legScale),
            StaticMeshComponent{MeshPrimitive::Cube},
            legMaterial
        );

        createSceneEntity(
            m_registry,
            "South East Leg",
            makeTransform(glm::vec3(legOffsetX, -0.47f, legOffsetZ), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), legScale),
            StaticMeshComponent{MeshPrimitive::Cube},
            legMaterial
        );

        for (std::size_t lightIndex = 0; lightIndex < lightRig.positions.size(); ++lightIndex)
        {
            createSceneEntity(
                m_registry,
                "Lamp Globe " + std::to_string(lightIndex + 1),
                makeTransform(
                    lightRig.positions[lightIndex],
                    glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                    glm::vec3(1.75f)
                ),
                StaticMeshComponent{MeshPrimitive::Sphere},
                lampMaterial
            );
        }

        m_registry.emplace<TableBoundsComponent>(table, TableBoundsComponent{
            .halfWidth = halfWidth,
            .halfDepth = halfDepth,
            .railRestitution = 0.92f,
            .ballRestitution = 0.96f,
            .railContactFrictionCoefficient = 0.14f,
            .ballContactFrictionCoefficient = 0.05f,
            .slidingFrictionCoefficient = 0.20f,
            .rollingFrictionCoefficient = 0.010f,
            .spinningFrictionCoefficient = 0.015f,
            .stopSpeedThreshold = 0.006f,
            .cornerPocketRadius = 0.090f,
            .sidePocketRadius = 0.080f
        });

        {
            const MaterialComponent cueBallMaterial{
                .albedo = m_variant->cueBall.albedo,
                .specularStrength = m_variant->cueBall.specularStrength,
                .shininess = m_variant->cueBall.shininess,
                .surfaceType = MaterialSurfaceType::BallResin,
                .roughness = 0.08f,
                .reflectivity = 0.08f,
                .clearcoatStrength = 1.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };

            const Entity cueBall = createSceneEntity(
                m_registry,
                m_variant->cueBall.name,
                makeTransform(glm::vec3(0.0f, m_variant->table.ballRadius, 0.42f)),
                StaticMeshComponent{MeshPrimitive::Sphere},
                cueBallMaterial
            );

            m_cueBallEntity = cueBall;

            m_registry.emplace<BallComponent>(cueBall, BallComponent{
                .radius = m_variant->table.ballRadius,
                .massKg = m_variant->table.ballMassKg,
                .linearVelocity = glm::vec3(0.0f),
                .angularVelocity = glm::vec3(0.0f),
                .number = m_variant->cueBall.number,
                .ruleTag = m_variant->cueBall.ruleTag,
                .pocketed = false,
                .isCueBall = true
            });
        }

        const std::vector<glm::vec3> rackPositions = buildRackPositions(*m_variant);

        for (std::size_t i = 0; i < m_variant->objectBalls.size(); ++i)
        {
            const BallSpawnDefinition& definition = m_variant->objectBalls[i];

            const MaterialComponent ballMaterial{
                .albedo = definition.albedo,
                .specularStrength = definition.specularStrength,
                .shininess = definition.shininess,
                .surfaceType = MaterialSurfaceType::BallResin,
                .roughness = 0.10f,
                .reflectivity = 0.075f,
                .clearcoatStrength = 1.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };

            const Entity ball = createSceneEntity(
                m_registry,
                definition.name,
                makeTransform(rackPositions[i]),
                StaticMeshComponent{MeshPrimitive::Sphere},
                ballMaterial
            );

            m_objectBallEntities.push_back(ball);

            m_registry.emplace<BallComponent>(ball, BallComponent{
                .radius = m_variant->table.ballRadius,
                .massKg = m_variant->table.ballMassKg,
                .linearVelocity = glm::vec3(0.0f),
                .angularVelocity = glm::vec3(0.0f),
                .number = definition.number,
                .ruleTag = definition.ruleTag,
                .pocketed = false,
                .isCueBall = definition.isCueBall
            });
        }

        m_cameraEntity = m_registry.createEntity();
        m_registry.emplace<NameComponent>(m_cameraEntity, NameComponent{"Main Camera"});
        m_registry.emplace<TransformComponent>(
            m_cameraEntity,
            makeTransform(
                glm::vec3(0.0f, 1.14f, 2.10f),
                glm::quat(glm::vec3(glm::radians(-27.0f), 0.0f, 0.0f))
            )
        );
        m_registry.emplace<CameraComponent>(m_cameraEntity, CameraComponent{
            .verticalFieldOfViewRadians = glm::radians(52.0f),
            .nearPlane = 0.05f,
            .farPlane = 60.0f
        });
        m_registry.emplace<CameraTagComponent>(m_cameraEntity, CameraTagComponent{});

        resetMatchToOpeningRack();
        m_shellState = ApplicationShellState::MainMenu;
        updateCameraRig(FIXED_TIME_STEP);
    }

    int Application::run()
    {
        m_timer.reset();

        while (!m_window.shouldClose())
        {
            m_window.pollEvents();
            processPlatformInput();

            double frameTime = m_timer.tick();
            frameTime = std::min(frameTime, MAX_FRAME_TIME);

            m_accumulator += frameTime;
            std::uint32_t fixedStepsThisFrame = 0;

            while (m_accumulator >= FIXED_TIME_STEP)
            {
                updateFixed(FIXED_TIME_STEP);
                m_accumulator -= FIXED_TIME_STEP;
                m_simulationTime += FIXED_TIME_STEP;
                ++m_fixedFrameIndex;
                ++fixedStepsThisFrame;
            }

            const double alpha = m_accumulator / FIXED_TIME_STEP;
            render(alpha);
            updateWindowTitle(frameTime, fixedStepsThisFrame);

            m_window.swapBuffers();
        }

        return 0;
    }

    void Application::resetMatchToOpeningRack()
    {
        m_matchState = MatchState{};
        m_matchState.discipline = m_variant->discipline;
        m_matchState.flowPhase = MatchFlowPhase::BreakShot;
        m_matchState.activePlayerIndex = 0;
        m_matchState.winnerPlayerIndex = -1;
        m_matchState.shotInProgress = false;
        m_matchState.foulCommittedThisTurn = false;
        m_matchState.ballInHand = false;

        m_shotState = ShotState{};
        m_currentShotResult.clear();
        m_spaceWasDownLastFrame = false;

        resetCueBall();

        const std::vector<glm::vec3> rackPositions = buildRackPositions(*m_variant);
        const glm::quat identityRotation(1.0f, 0.0f, 0.0f, 0.0f);

        for (std::size_t i = 0; i < m_objectBallEntities.size(); ++i)
        {
            TransformComponent* transform = m_registry.tryGet<TransformComponent>(m_objectBallEntities[i]);
            BallComponent* ball = m_registry.tryGet<BallComponent>(m_objectBallEntities[i]);

            if ((transform == nullptr) || (ball == nullptr))
            {
                continue;
            }

            transform->position = rackPositions[i];
            transform->previousPosition = transform->position;
            transform->rotation = identityRotation;
            transform->previousRotation = transform->rotation;

            ball->linearVelocity = glm::vec3(0.0f);
            ball->angularVelocity = glm::vec3(0.0f);
            ball->pocketed = false;
        }

        m_titleUpdateAccumulator = TITLE_UPDATE_INTERVAL_SECONDS;
    }

    void Application::applyMainMenuSelection()
    {
        switch (m_mainMenuSelection)
        {
            case MainMenuSelection::StartMatch:
                resetMatchToOpeningRack();
                m_shellState = ApplicationShellState::Gameplay;
                break;

            case MainMenuSelection::Quit:
                m_window.requestClose();
                break;
        }

        m_titleUpdateAccumulator = TITLE_UPDATE_INTERVAL_SECONDS;
    }

    void Application::applyPauseMenuSelection()
    {
        switch (m_pauseMenuSelection)
        {
            case PauseMenuSelection::Resume:
                m_shellState = ApplicationShellState::Gameplay;
                break;

            case PauseMenuSelection::RestartRack:
                resetMatchToOpeningRack();
                m_shellState = ApplicationShellState::Gameplay;
                break;

            case PauseMenuSelection::ReturnToMainMenu:
                m_shellState = ApplicationShellState::MainMenu;
                break;
        }

        m_pauseMenuSelection = PauseMenuSelection::Resume;
        m_titleUpdateAccumulator = TITLE_UPDATE_INTERVAL_SECONDS;
    }

    void Application::setCameraViewMode(CameraViewMode mode)
    {
        m_cameraRigState.mode = mode;
        m_titleUpdateAccumulator = TITLE_UPDATE_INTERVAL_SECONDS;
    }

    CameraRigContext Application::buildGameplayCameraContext() const
    {
        CameraRigContext context;
        context.tableCenter = glm::vec3(0.0f, m_variant->table.ballRadius, 0.0f);
        context.aimDirection = aimDirectionFromAngle(m_shotState.aimAngleRadians);
        context.ballsInMotion = Physics::anyBallInMotion(const_cast<Registry&>(m_registry));

        float fastestSpeedSquared = 0.0f;

        const_cast<Registry&>(m_registry).view<TransformComponent, BallComponent>().each(
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
        TransformComponent* cameraTransform = m_registry.tryGet<TransformComponent>(m_cameraEntity);
        if (cameraTransform == nullptr)
        {
            return;
        }

        CameraPose targetPose;

        if (m_shellState == ApplicationShellState::Gameplay)
        {
            if (m_cameraRigState.mode == CameraViewMode::FreeLook)
            {
                applyCameraRigInput(
                    m_cameraRigState,
                    CameraRigInputAxes{
                        .orbitYaw = m_cameraOrbitYawInput,
                        .orbitPitch = m_cameraOrbitPitchInput,
                        .zoom = m_cameraZoomInput
                    },
                    static_cast<float>(deltaTimeSeconds)
                );
            }

            targetPose = desiredCameraPose(m_cameraRigState, buildGameplayCameraContext());
        }
        else
        {
            CameraRigState menuRigState;
            menuRigState.mode = CameraViewMode::TableOverview;
            targetPose = desiredCameraPose(
                menuRigState,
                CameraRigContext{
                    .tableCenter = glm::vec3(0.0f, m_variant->table.ballRadius, 0.0f),
                    .cueBallPosition = glm::vec3(0.0f, m_variant->table.ballRadius, 0.42f),
                    .trackedBallPosition = glm::vec3(0.0f, m_variant->table.ballRadius, 0.42f),
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

    void Application::processPlatformInput()
    {
        GLFWwindow* handle = m_window.nativeHandle();
        const bool escapeDown = glfwGetKey(handle, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        const bool qualityToggleDown = glfwGetKey(handle, GLFW_KEY_F2) == GLFW_PRESS;
        const bool titleStatsToggleDown = glfwGetKey(handle, GLFW_KEY_F1) == GLFW_PRESS;
        const bool menuUpDown =
            (glfwGetKey(handle, GLFW_KEY_UP) == GLFW_PRESS) ||
            (glfwGetKey(handle, GLFW_KEY_W) == GLFW_PRESS);
        const bool menuDownDown =
            (glfwGetKey(handle, GLFW_KEY_DOWN) == GLFW_PRESS) ||
            (glfwGetKey(handle, GLFW_KEY_S) == GLFW_PRESS);
        const bool menuConfirmDown =
            (glfwGetKey(handle, GLFW_KEY_ENTER) == GLFW_PRESS) ||
            (glfwGetKey(handle, GLFW_KEY_KP_ENTER) == GLFW_PRESS);
        const bool cameraCycleDown = glfwGetKey(handle, GLFW_KEY_TAB) == GLFW_PRESS;
        const bool cameraAimDown = glfwGetKey(handle, GLFW_KEY_1) == GLFW_PRESS;
        const bool cameraOverviewDown = glfwGetKey(handle, GLFW_KEY_2) == GLFW_PRESS;
        const bool cameraFollowDown = glfwGetKey(handle, GLFW_KEY_3) == GLFW_PRESS;
        const bool cameraFreeLookDown = glfwGetKey(handle, GLFW_KEY_4) == GLFW_PRESS;
        const bool freeLookLeftDown = glfwGetKey(handle, GLFW_KEY_J) == GLFW_PRESS;
        const bool freeLookRightDown = glfwGetKey(handle, GLFW_KEY_L) == GLFW_PRESS;
        const bool freeLookUpDown = glfwGetKey(handle, GLFW_KEY_I) == GLFW_PRESS;
        const bool freeLookDownDown = glfwGetKey(handle, GLFW_KEY_K) == GLFW_PRESS;
        const bool freeLookZoomInDown = glfwGetKey(handle, GLFW_KEY_U) == GLFW_PRESS;
        const bool freeLookZoomOutDown = glfwGetKey(handle, GLFW_KEY_O) == GLFW_PRESS;

        const bool escapePressed = escapeDown && !m_escapeWasDownLastFrame;
        const bool qualityTogglePressed = qualityToggleDown && !m_qualityToggleWasDownLastFrame;
        const bool titleStatsTogglePressed = titleStatsToggleDown && !m_titleStatsToggleWasDownLastFrame;
        const bool menuUpPressed = menuUpDown && !m_menuUpWasDownLastFrame;
        const bool menuDownPressed = menuDownDown && !m_menuDownWasDownLastFrame;
        const bool menuConfirmPressed = menuConfirmDown && !m_menuConfirmWasDownLastFrame;
        const bool cameraCyclePressed = cameraCycleDown && !m_cameraCycleWasDownLastFrame;
        const bool cameraAimPressed = cameraAimDown && !m_cameraAimWasDownLastFrame;
        const bool cameraOverviewPressed = cameraOverviewDown && !m_cameraOverviewWasDownLastFrame;
        const bool cameraFollowPressed = cameraFollowDown && !m_cameraFollowWasDownLastFrame;
        const bool cameraFreeLookPressed = cameraFreeLookDown && !m_cameraFreeLookWasDownLastFrame;

        auto latchInputEdges = [&]()
        {
            m_escapeWasDownLastFrame = escapeDown;
            m_qualityToggleWasDownLastFrame = qualityToggleDown;
            m_titleStatsToggleWasDownLastFrame = titleStatsToggleDown;
            m_menuUpWasDownLastFrame = menuUpDown;
            m_menuDownWasDownLastFrame = menuDownDown;
            m_menuConfirmWasDownLastFrame = menuConfirmDown;
            m_cameraCycleWasDownLastFrame = cameraCycleDown;
            m_cameraAimWasDownLastFrame = cameraAimDown;
            m_cameraOverviewWasDownLastFrame = cameraOverviewDown;
            m_cameraFollowWasDownLastFrame = cameraFollowDown;
            m_cameraFreeLookWasDownLastFrame = cameraFreeLookDown;
        };

        auto clearCameraInputAxes = [&]()
        {
            m_cameraOrbitYawInput = 0.0f;
            m_cameraOrbitPitchInput = 0.0f;
            m_cameraZoomInput = 0.0f;
        };

        if (qualityTogglePressed)
        {
            switch (m_renderQuality)
            {
                case RenderQualityPreset::Low:
                    m_renderQuality = RenderQualityPreset::Balanced;
                    break;

                case RenderQualityPreset::Balanced:
                    m_renderQuality = RenderQualityPreset::High;
                    break;

                case RenderQualityPreset::High:
                    m_renderQuality = RenderQualityPreset::Low;
                    break;
            }

            m_titleUpdateAccumulator = TITLE_UPDATE_INTERVAL_SECONDS;
        }

        if (titleStatsTogglePressed)
        {
            m_showPerformanceStatsInTitle = !m_showPerformanceStatsInTitle;
            m_titleUpdateAccumulator = TITLE_UPDATE_INTERVAL_SECONDS;
        }

        if (m_shellState == ApplicationShellState::MainMenu)
        {
            if (escapePressed)
            {
                m_window.requestClose();
            }

            if (menuUpPressed || menuDownPressed)
            {
                m_mainMenuSelection =
                    (m_mainMenuSelection == MainMenuSelection::StartMatch)
                    ? MainMenuSelection::Quit
                    : MainMenuSelection::StartMatch;
                m_titleUpdateAccumulator = TITLE_UPDATE_INTERVAL_SECONDS;
            }

            if (menuConfirmPressed)
            {
                applyMainMenuSelection();
            }

            clearCameraInputAxes();
            m_spaceWasDownLastFrame = false;
            latchInputEdges();
            return;
        }

        if (m_shellState == ApplicationShellState::PauseMenu)
        {
            if (escapePressed)
            {
                m_shellState = ApplicationShellState::Gameplay;
                m_pauseMenuSelection = PauseMenuSelection::Resume;
                m_titleUpdateAccumulator = TITLE_UPDATE_INTERVAL_SECONDS;
            }

            if (menuUpPressed)
            {
                switch (m_pauseMenuSelection)
                {
                    case PauseMenuSelection::Resume:
                        m_pauseMenuSelection = PauseMenuSelection::ReturnToMainMenu;
                        break;

                    case PauseMenuSelection::RestartRack:
                        m_pauseMenuSelection = PauseMenuSelection::Resume;
                        break;

                    case PauseMenuSelection::ReturnToMainMenu:
                        m_pauseMenuSelection = PauseMenuSelection::RestartRack;
                        break;
                }

                m_titleUpdateAccumulator = TITLE_UPDATE_INTERVAL_SECONDS;
            }

            if (menuDownPressed)
            {
                switch (m_pauseMenuSelection)
                {
                    case PauseMenuSelection::Resume:
                        m_pauseMenuSelection = PauseMenuSelection::RestartRack;
                        break;

                    case PauseMenuSelection::RestartRack:
                        m_pauseMenuSelection = PauseMenuSelection::ReturnToMainMenu;
                        break;

                    case PauseMenuSelection::ReturnToMainMenu:
                        m_pauseMenuSelection = PauseMenuSelection::Resume;
                        break;
                }

                m_titleUpdateAccumulator = TITLE_UPDATE_INTERVAL_SECONDS;
            }

            if (menuConfirmPressed)
            {
                applyPauseMenuSelection();
            }

            clearCameraInputAxes();
            m_spaceWasDownLastFrame = false;
            latchInputEdges();
            return;
        }

        if (escapePressed)
        {
            m_shellState = ApplicationShellState::PauseMenu;
            m_pauseMenuSelection = PauseMenuSelection::Resume;
            clearCameraInputAxes();
            m_spaceWasDownLastFrame = false;
            m_titleUpdateAccumulator = TITLE_UPDATE_INTERVAL_SECONDS;
            latchInputEdges();
            return;
        }

        if (cameraCyclePressed)
        {
            switch (m_cameraRigState.mode)
            {
                case CameraViewMode::PlayerAim:
                    setCameraViewMode(CameraViewMode::TableOverview);
                    break;

                case CameraViewMode::TableOverview:
                    setCameraViewMode(CameraViewMode::ShotFollow);
                    break;

                case CameraViewMode::ShotFollow:
                    setCameraViewMode(CameraViewMode::FreeLook);
                    break;

                case CameraViewMode::FreeLook:
                    setCameraViewMode(CameraViewMode::PlayerAim);
                    break;
            }
        }

        if (cameraAimPressed)
        {
            setCameraViewMode(CameraViewMode::PlayerAim);
        }

        if (cameraOverviewPressed)
        {
            setCameraViewMode(CameraViewMode::TableOverview);
        }

        if (cameraFollowPressed)
        {
            setCameraViewMode(CameraViewMode::ShotFollow);
        }

        if (cameraFreeLookPressed)
        {
            setCameraViewMode(CameraViewMode::FreeLook);
        }

        clearCameraInputAxes();
        if (m_cameraRigState.mode == CameraViewMode::FreeLook)
        {
            if (freeLookLeftDown)
            {
                m_cameraOrbitYawInput -= 1.0f;
            }

            if (freeLookRightDown)
            {
                m_cameraOrbitYawInput += 1.0f;
            }

            if (freeLookUpDown)
            {
                m_cameraOrbitPitchInput += 1.0f;
            }

            if (freeLookDownDown)
            {
                m_cameraOrbitPitchInput -= 1.0f;
            }

            if (freeLookZoomInDown)
            {
                m_cameraZoomInput += 1.0f;
            }

            if (freeLookZoomOutDown)
            {
                m_cameraZoomInput -= 1.0f;
            }
        }

        if (glfwGetKey(handle, GLFW_KEY_R) == GLFW_PRESS)
        {
            resetCueBall();
            m_shotState.phase = ShotPhase::Aiming;
            m_shotState.charge01 = 0.0f;
            m_matchState.shotInProgress = false;
            m_currentShotResult.clear();
        }

        if ((m_matchState.flowPhase == MatchFlowPhase::FrameOver) ||
            (m_shotState.phase == ShotPhase::BallsInMotion))
        {
            m_spaceWasDownLastFrame = glfwGetKey(handle, GLFW_KEY_SPACE) == GLFW_PRESS;
            latchInputEdges();
            return;
        }

        constexpr float AIM_SPEED = 1.8f;
        constexpr float STRIKE_ADJUST_SPEED = 1.8f;
        constexpr float MAX_STRIKE_RADIUS01 = 0.75f;

        if (glfwGetKey(handle, GLFW_KEY_A) == GLFW_PRESS)
        {
            m_shotState.aimAngleRadians += static_cast<float>(FIXED_TIME_STEP) * AIM_SPEED;
        }

        if (glfwGetKey(handle, GLFW_KEY_D) == GLFW_PRESS)
        {
            m_shotState.aimAngleRadians -= static_cast<float>(FIXED_TIME_STEP) * AIM_SPEED;
        }

        if (glfwGetKey(handle, GLFW_KEY_LEFT) == GLFW_PRESS)
        {
            m_shotState.strikeRight01 -= static_cast<float>(FIXED_TIME_STEP) * STRIKE_ADJUST_SPEED;
        }

        if (glfwGetKey(handle, GLFW_KEY_RIGHT) == GLFW_PRESS)
        {
            m_shotState.strikeRight01 += static_cast<float>(FIXED_TIME_STEP) * STRIKE_ADJUST_SPEED;
        }

        if (glfwGetKey(handle, GLFW_KEY_UP) == GLFW_PRESS)
        {
            m_shotState.strikeForward01 += static_cast<float>(FIXED_TIME_STEP) * STRIKE_ADJUST_SPEED;
        }

        if (glfwGetKey(handle, GLFW_KEY_DOWN) == GLFW_PRESS)
        {
            m_shotState.strikeForward01 -= static_cast<float>(FIXED_TIME_STEP) * STRIKE_ADJUST_SPEED;
        }

        if (glfwGetKey(handle, GLFW_KEY_C) == GLFW_PRESS)
        {
            m_shotState.strikeRight01 = 0.0f;
            m_shotState.strikeForward01 = 0.0f;
        }

        const glm::vec2 clampedStrike = clampStrikeOffset(
            m_shotState.strikeRight01,
            m_shotState.strikeForward01,
            MAX_STRIKE_RADIUS01
        );

        m_shotState.strikeRight01 = clampedStrike.x;
        m_shotState.strikeForward01 = clampedStrike.y;

        const bool spaceDown = glfwGetKey(handle, GLFW_KEY_SPACE) == GLFW_PRESS;

        if ((m_shotState.phase == ShotPhase::Aiming) && spaceDown)
        {
            m_shotState.phase = ShotPhase::Charging;
        }

        if (m_shotState.phase == ShotPhase::Charging)
        {
            if (spaceDown)
            {
                m_shotState.charge01 = std::min(m_shotState.charge01 + 0.015f, 1.0f);
            }
            else if (m_spaceWasDownLastFrame)
            {
                const bool shotFired = fireCurrentShot();
                m_shotState.phase = shotFired ? ShotPhase::BallsInMotion : ShotPhase::Aiming;
                m_shotState.charge01 = 0.0f;
                m_matchState.shotInProgress = shotFired;
            }
        }

        m_spaceWasDownLastFrame = spaceDown;
        latchInputEdges();
    }

    Entity Application::findCueBall() const
    {
        if (m_cueBallEntity.isValid())
        {
            return m_cueBallEntity;
        }

        Entity found {};

        const_cast<Registry&>(m_registry).view<BallComponent>().each(
            [&](Entity entity, BallComponent& ball)
            {
                if (ball.isCueBall)
                {
                    found = entity;
                }
            }
        );

        return found;
    }

    void Application::resetCueBall()
    {
        const Entity cueBall = findCueBall();
        if (!cueBall.isValid())
        {
            return;
        }

        TransformComponent* transform = m_registry.tryGet<TransformComponent>(cueBall);
        BallComponent* ball = m_registry.tryGet<BallComponent>(cueBall);

        if ((transform == nullptr) || (ball == nullptr))
        {
            return;
        }

        transform->position = glm::vec3(0.0f, ball->radius, 0.42f);
        transform->previousPosition = transform->position;
        transform->rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        transform->previousRotation = transform->rotation;

        ball->linearVelocity = glm::vec3(0.0f);
        ball->angularVelocity = glm::vec3(0.0f);
        ball->pocketed = false;

        m_shotState.strikeRight01 = 0.0f;
        m_shotState.strikeForward01 = 0.0f;
    }

    bool Application::fireCurrentShot()
    {
        const Entity cueBall = findCueBall();
        if (!cueBall.isValid())
        {
            return false;
        }

        BallComponent* ball = m_registry.tryGet<BallComponent>(cueBall);
        if ((ball == nullptr) || ball->pocketed)
        {
            return false;
        }

        m_currentShotResult.clear();
        m_currentShotResult.shotActive = true;

        const glm::vec3 forward = aimDirectionFromAngle(m_shotState.aimAngleRadians);
        const glm::vec3 up(0.0f, 1.0f, 0.0f);
        const glm::vec3 right = glm::normalize(glm::cross(up, forward));

        constexpr float MIN_SHOT_SPEED = 0.4f;
        constexpr float MAX_SHOT_SPEED = 3.8f;

        const float shotSpeed =
            MIN_SHOT_SPEED + (MAX_SHOT_SPEED - MIN_SHOT_SPEED) * m_shotState.charge01;

        ball->linearVelocity = forward * shotSpeed;

        const float spinBase = shotSpeed / ball->radius;

        const glm::vec3 sideSpin =
            up * (m_shotState.strikeRight01 * 0.85f * spinBase);

        const glm::vec3 topBackSpin =
            right * (m_shotState.strikeForward01 * 1.00f * spinBase);

        ball->angularVelocity = sideSpin + topBackSpin;
        return true;
    }

    void Application::updateFixed(double deltaTimeSeconds)
    {
        if (m_shellState != ApplicationShellState::Gameplay)
        {
            updateCameraRig(deltaTimeSeconds);
            return;
        }

        Physics::stepBilliardsWorld(m_registry, deltaTimeSeconds, m_currentShotResult);

        if ((m_shotState.phase == ShotPhase::BallsInMotion) && !Physics::anyBallInMotion(m_registry))
        {
            Rules::resolveShot(*m_variant, m_matchState, m_registry, m_currentShotResult);

            if ((m_matchState.flowPhase != MatchFlowPhase::FrameOver) && m_matchState.ballInHand)
            {
                resetCueBall();
            }

            if (m_matchState.flowPhase != MatchFlowPhase::FrameOver)
            {
                m_shotState.phase = ShotPhase::Aiming;
            }

            m_currentShotResult.clear();
        }

        updateCameraRig(deltaTimeSeconds);
    }

    void Application::render(double alpha)
    {
        glViewport(0, 0, m_window.width(), m_window.height());

        if (m_shellState == ApplicationShellState::MainMenu)
        {
            glClearColor(0.025f, 0.020f, 0.022f, 1.0f);
        }
        else if (m_shellState == ApplicationShellState::PauseMenu)
        {
            glClearColor(0.022f, 0.020f, 0.024f, 1.0f);
        }
        else if (m_matchState.flowPhase == MatchFlowPhase::FrameOver)
        {
            glClearColor(0.03f, 0.025f, 0.03f, 1.0f);
        }
        else
        {
            glClearColor(0.035f, 0.025f, 0.02f, 1.0f);
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const TransformComponent* cameraTransform = m_registry.tryGet<TransformComponent>(m_cameraEntity);
        const CameraComponent* camera = m_registry.tryGet<CameraComponent>(m_cameraEntity);

        if ((cameraTransform == nullptr) || (camera == nullptr))
        {
            return;
        }

        const InterpolatedTransform interpolatedCamera =
            interpolateTransform(*cameraTransform, static_cast<float>(alpha));

        const glm::vec3 cameraForward =
            interpolatedCamera.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        const glm::vec3 cameraUp =
            interpolatedCamera.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec3 cameraRight =
            interpolatedCamera.rotation * glm::vec3(1.0f, 0.0f, 0.0f);

        const UiOverlayFrame overlayFrame{
            .rotation = interpolatedCamera.rotation,
            .right = cameraRight,
            .up = cameraUp
        };

        const glm::mat4 view = Camera::viewMatrix(
            interpolatedCamera.position,
            cameraForward,
            cameraUp
        );

        const glm::mat4 projection = Camera::projectionMatrix(
            camera->verticalFieldOfViewRadians,
            m_window.aspectRatio(),
            camera->nearPlane,
            camera->farPlane
        );

        const PointLightRig lightRig = buildSaloonLightRig();

        m_basicShader->bind();
        m_basicShader->setMat4("uView", view);
        m_basicShader->setMat4("uProjection", projection);
        m_basicShader->setVec3("uViewPosition", interpolatedCamera.position);

        m_basicShader->setVec3(
            "uDirectionalLightDirection",
            glm::normalize(glm::vec3(-0.35f, -1.0f, -0.18f))
        );
        m_basicShader->setVec3(
            "uDirectionalLightColor",
            glm::vec3(0.22f, 0.24f, 0.28f)
        );

        int activePointLightCount = 2;
        float reflectionScale = 0.55f;
        float emissionScale = 0.85f;

        switch (m_renderQuality)
        {
            case RenderQualityPreset::Low:
                activePointLightCount = 1;
                reflectionScale = 0.15f;
                emissionScale = 0.70f;
                break;

            case RenderQualityPreset::Balanced:
                activePointLightCount = 2;
                reflectionScale = 0.55f;
                emissionScale = 0.85f;
                break;

            case RenderQualityPreset::High:
                activePointLightCount = 3;
                reflectionScale = 1.0f;
                emissionScale = 1.0f;
                break;
        }

        m_basicShader->setInt("uActivePointLightCount", activePointLightCount);
        m_basicShader->setFloat("uReflectionScale", reflectionScale);
        m_basicShader->setFloat("uEmissionScale", emissionScale);

        for (std::size_t lightIndex = 0; lightIndex < lightRig.positions.size(); ++lightIndex)
        {
            m_basicShader->setVec3(
                "uPointLightPositions[" + std::to_string(lightIndex) + "]",
                lightRig.positions[lightIndex]
            );
            m_basicShader->setVec3(
                "uPointLightColors[" + std::to_string(lightIndex) + "]",
                lightRig.colors[lightIndex]
            );
        }

        auto bindMaterial = [&](const MaterialComponent& material, const glm::vec3& dynamicEmission, int visualType)
        {
            m_basicShader->setVec3("uMaterialAlbedo", material.albedo);
            m_basicShader->setFloat("uMaterialSpecularStrength", material.specularStrength);
            m_basicShader->setFloat("uMaterialShininess", material.shininess);
            m_basicShader->setInt("uMaterialSurfaceType", static_cast<int>(material.surfaceType));
            m_basicShader->setFloat("uMaterialRoughness", material.roughness);
            m_basicShader->setFloat("uMaterialReflectivity", material.reflectivity);
            m_basicShader->setFloat("uMaterialClearcoatStrength", material.clearcoatStrength);
            m_basicShader->setVec3(
                "uEmissionColor",
                material.emissionColor * material.emissionIntensity + dynamicEmission
            );
            m_basicShader->setInt("uBallVisualType", visualType);
        };

        const Entity cueBall = findCueBall();
        glm::vec3 cueBallPosition(0.0f);
        bool cueBallVisible = false;

        if (cueBall.isValid())
        {
            if (const TransformComponent* cueBallTransform = m_registry.tryGet<TransformComponent>(cueBall))
            {
                if (const BallComponent* cueBallBall = m_registry.tryGet<BallComponent>(cueBall))
                {
                    cueBallVisible = !cueBallBall->pocketed;
                }

                cueBallPosition =
                    interpolateTransform(*cueBallTransform, static_cast<float>(alpha)).position;
            }
        }

        m_registry.view<TransformComponent, StaticMeshComponent, MaterialComponent>().each(
            [&](Entity entity, TransformComponent& transform, StaticMeshComponent& meshComponent, MaterialComponent& material)
            {
                const BallComponent* ball = m_registry.tryGet<BallComponent>(entity);
                if ((ball != nullptr) && ball->pocketed)
                {
                    return;
                }

                const glm::mat4 model =
                    composeInterpolatedMatrix(transform, static_cast<float>(alpha));

                Mesh* mesh = nullptr;

                switch (meshComponent.primitive)
                {
                    case MeshPrimitive::Cube:
                        mesh = m_cubeMesh.get();
                        break;
                    case MeshPrimitive::Plane:
                        mesh = m_planeMesh.get();
                        break;
                    case MeshPrimitive::Sphere:
                        mesh = m_sphereMesh.get();
                        break;
                }

                if (mesh == nullptr)
                {
                    return;
                }

                const bool highlightCueBall =
                    (entity == cueBall) &&
                    (m_shellState == ApplicationShellState::Gameplay) &&
                    (m_shotState.phase != ShotPhase::BallsInMotion) &&
                    (m_matchState.flowPhase != MatchFlowPhase::FrameOver);

                glm::vec3 dynamicEmission(0.0f);
                if (highlightCueBall)
                {
                    const float glow = 0.05f + 0.18f * m_shotState.charge01;
                    dynamicEmission = glm::vec3(glow, glow, glow * 0.82f);
                }

                m_basicShader->setMat4("uModel", model);
                bindMaterial(material, dynamicEmission, (ball != nullptr) ? ballVisualType(*ball) : 0);
                mesh->draw();
            }
        );

        if ((m_shellState == ApplicationShellState::Gameplay) &&
            (m_shotState.phase != ShotPhase::BallsInMotion) &&
            cueBall.isValid() &&
            cueBallVisible &&
            (m_matchState.flowPhase != MatchFlowPhase::FrameOver))
        {
            const glm::vec3 aimDirection = aimDirectionFromAngle(m_shotState.aimAngleRadians);

            TransformComponent guideTransform = makeTransform(
                cueBallPosition + aimDirection * 0.18f,
                glm::quat(glm::vec3(0.0f, -m_shotState.aimAngleRadians, 0.0f)),
                glm::vec3(0.03f, 0.03f, 0.18f + 0.35f * m_shotState.charge01)
            );

            const MaterialComponent guideMaterial{
                .albedo = glm::vec3(0.92f, 0.82f, 0.42f),
                .specularStrength = 0.20f,
                .shininess = 16.0f,
                .surfaceType = MaterialSurfaceType::Generic,
                .roughness = 0.38f,
                .reflectivity = 0.04f,
                .clearcoatStrength = 0.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };

            m_basicShader->setMat4("uModel", composeMatrix(
                guideTransform.position,
                guideTransform.rotation,
                guideTransform.scale
            ));
            bindMaterial(
                guideMaterial,
                glm::vec3(0.10f, 0.08f, 0.02f) + glm::vec3(0.10f, 0.06f, 0.01f) * m_shotState.charge01,
                0
            );
            m_cubeMesh->draw();

            const glm::vec3 up(0.0f, 1.0f, 0.0f);
            const glm::vec3 right = glm::normalize(glm::cross(up, aimDirection));

            const BallComponent* cueBallBall = m_registry.tryGet<BallComponent>(cueBall);
            if (cueBallBall != nullptr)
            {
                const float markerRadius = cueBallBall->radius * 0.72f;

                const glm::vec3 markerPosition =
                    cueBallPosition +
                    right * (m_shotState.strikeRight01 * markerRadius) +
                    aimDirection * (m_shotState.strikeForward01 * markerRadius) +
                    glm::vec3(0.0f, cueBallBall->radius * 0.25f, 0.0f);

                const MaterialComponent markerMaterial{
                    .albedo = glm::vec3(0.10f, 0.10f, 0.12f),
                    .specularStrength = 0.10f,
                    .shininess = 12.0f,
                    .surfaceType = MaterialSurfaceType::Generic,
                    .roughness = 0.52f,
                    .reflectivity = 0.03f,
                    .clearcoatStrength = 0.0f,
                    .emissionColor = glm::vec3(0.0f),
                    .emissionIntensity = 0.0f
                };

                const TransformComponent markerTransform = makeTransform(
                    markerPosition,
                    glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                    glm::vec3(0.012f, 0.012f, 0.012f)
                );

                m_basicShader->setMat4("uModel", composeMatrix(
                    markerTransform.position,
                    markerTransform.rotation,
                    markerTransform.scale
                ));
                bindMaterial(markerMaterial, glm::vec3(0.18f, 0.12f, 0.02f), 0);
                m_cubeMesh->draw();
            }
        }

        if (m_shellState == ApplicationShellState::Gameplay)
        {
            const glm::vec3 hudLeft =
                interpolatedCamera.position +
                cameraForward * 1.10f +
                cameraUp * 0.50f -
                cameraRight * 0.82f;

            const MaterialComponent panelMaterial{
                .albedo = glm::vec3(0.05f, 0.045f, 0.040f),
                .specularStrength = 0.04f,
                .shininess = 8.0f,
                .surfaceType = MaterialSurfaceType::Generic,
                .roughness = 0.95f,
                .reflectivity = 0.02f,
                .clearcoatStrength = 0.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };

            const MaterialComponent titleTextMaterial{
                .albedo = glm::vec3(0.95f, 0.90f, 0.76f),
                .specularStrength = 0.10f,
                .shininess = 12.0f,
                .surfaceType = MaterialSurfaceType::Generic,
                .roughness = 0.32f,
                .reflectivity = 0.04f,
                .clearcoatStrength = 0.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };

            const MaterialComponent bodyTextMaterial{
                .albedo = glm::vec3(0.90f, 0.86f, 0.80f),
                .specularStrength = 0.07f,
                .shininess = 10.0f,
                .surfaceType = MaterialSurfaceType::Generic,
                .roughness = 0.50f,
                .reflectivity = 0.03f,
                .clearcoatStrength = 0.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };

            const MaterialComponent accentTextMaterial{
                .albedo = glm::vec3(0.84f, 0.68f, 0.36f),
                .specularStrength = 0.08f,
                .shininess = 12.0f,
                .surfaceType = MaterialSurfaceType::Generic,
                .roughness = 0.42f,
                .reflectivity = 0.03f,
                .clearcoatStrength = 0.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };

            const MaterialComponent hintTextMaterial{
                .albedo = glm::vec3(0.74f, 0.67f, 0.58f),
                .specularStrength = 0.04f,
                .shininess = 8.0f,
                .surfaceType = MaterialSurfaceType::Generic,
                .roughness = 0.88f,
                .reflectivity = 0.02f,
                .clearcoatStrength = 0.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };

            const MaterialComponent chargeTrackMaterial{
                .albedo = glm::vec3(0.14f, 0.10f, 0.08f),
                .specularStrength = 0.04f,
                .shininess = 8.0f,
                .surfaceType = MaterialSurfaceType::Generic,
                .roughness = 0.88f,
                .reflectivity = 0.02f,
                .clearcoatStrength = 0.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };

            const MaterialComponent chargeFillMaterial{
                .albedo = glm::vec3(0.82f, 0.56f, 0.20f),
                .specularStrength = 0.10f,
                .shininess = 14.0f,
                .surfaceType = MaterialSurfaceType::Generic,
                .roughness = 0.34f,
                .reflectivity = 0.04f,
                .clearcoatStrength = 0.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };

            const UiTextStyle titleStyle{
                .material = titleTextMaterial,
                .dynamicEmission = glm::vec3(0.030f, 0.020f, 0.008f),
                .cellSize = 0.0085f,
                .depth = 0.010f,
                .alignment = UiTextAlignment::Left
            };

            const UiTextStyle bodyStyle{
                .material = bodyTextMaterial,
                .dynamicEmission = glm::vec3(0.012f, 0.010f, 0.006f),
                .cellSize = 0.0065f,
                .depth = 0.008f,
                .alignment = UiTextAlignment::Left
            };

            const UiTextStyle accentStyle{
                .material = accentTextMaterial,
                .dynamicEmission = glm::vec3(0.022f, 0.014f, 0.006f),
                .cellSize = 0.0065f,
                .depth = 0.008f,
                .alignment = UiTextAlignment::Left
            };

            const UiTextStyle hintStyle{
                .material = hintTextMaterial,
                .dynamicEmission = glm::vec3(0.010f, 0.008f, 0.004f),
                .cellSize = 0.0055f,
                .depth = 0.007f,
                .alignment = UiTextAlignment::Left
            };

            const int activePlayerIndex =
                std::clamp(m_matchState.activePlayerIndex, 0, 1);

            const std::string disciplineLine =
                (m_variant != nullptr) ? m_variant->displayName : "BILLIARDS";
            const std::string playerLine =
                "PLAYER " + std::to_string(activePlayerIndex + 1);
            const std::string phaseLine =
                std::string("PHASE ") + shotPhaseLabel(m_shotState.phase);
            const std::string flowLine =
                std::string("TABLE ") + matchFlowPhaseLabel(m_matchState.flowPhase);
            const std::string groupLine =
                std::string("GROUP ") +
                playerTargetGroupLabel(m_matchState.players[activePlayerIndex].targetGroup);
            const std::string qualityLine =
                std::string("QUALITY ") + renderQualityLabel(m_renderQuality);
            const std::string cameraLine =
                std::string("CAMERA ") + cameraViewModeLabel(m_cameraRigState.mode);

            std::string statusLine = "STATUS READY";
            if (m_matchState.foulCommittedThisTurn)
            {
                statusLine = "FOUL RECORDED";
            }

            if (m_matchState.ballInHand)
            {
                statusLine = "BALL IN HAND";
            }

            if ((m_matchState.flowPhase == MatchFlowPhase::FrameOver) &&
                (m_matchState.winnerPlayerIndex >= 0))
            {
                statusLine =
                    "WINNER PLAYER " +
                    std::to_string(m_matchState.winnerPlayerIndex + 1);
            }

            const float panelWidth = 0.96f;
            const float chargeBarWidth = 0.60f;
            const float clampedCharge = std::clamp(m_shotState.charge01, 0.0f, 1.0f);
            const float chargeFillWidth = chargeBarWidth * clampedCharge;
            const glm::vec3 panelCenter =
                hudLeft +
                cameraRight * 0.44f -
                cameraUp * 0.12f +
                cameraForward * 0.10f;

            glDisable(GL_DEPTH_TEST);

            renderUiOverlayBox(
                *m_basicShader,
                *m_cubeMesh,
                overlayFrame,
                panelCenter,
                glm::vec3(panelWidth, 0.40f, 0.020f),
                panelMaterial,
                glm::vec3(0.010f, 0.008f, 0.008f)
            );

            renderUiOverlayText(
                *m_basicShader,
                *m_cubeMesh,
                overlayFrame,
                disciplineLine,
                hudLeft + cameraUp * 0.02f + cameraForward * 0.020f,
                titleStyle
            );

            renderUiOverlayText(
                *m_basicShader,
                *m_cubeMesh,
                overlayFrame,
                playerLine,
                hudLeft - cameraUp * 0.05f + cameraForward * 0.020f,
                accentStyle
            );

            renderUiOverlayText(
                *m_basicShader,
                *m_cubeMesh,
                overlayFrame,
                phaseLine,
                hudLeft - cameraUp * 0.12f + cameraForward * 0.020f,
                bodyStyle
            );

            renderUiOverlayText(
                *m_basicShader,
                *m_cubeMesh,
                overlayFrame,
                flowLine,
                hudLeft - cameraUp * 0.18f + cameraForward * 0.020f,
                bodyStyle
            );

            renderUiOverlayText(
                *m_basicShader,
                *m_cubeMesh,
                overlayFrame,
                groupLine,
                hudLeft - cameraUp * 0.24f + cameraForward * 0.020f,
                bodyStyle
            );

            renderUiOverlayText(
                *m_basicShader,
                *m_cubeMesh,
                overlayFrame,
                statusLine,
                hudLeft - cameraUp * 0.30f + cameraForward * 0.020f,
                bodyStyle
            );

            renderUiOverlayText(
                *m_basicShader,
                *m_cubeMesh,
                overlayFrame,
                qualityLine,
                hudLeft + cameraRight * 0.52f - cameraUp * 0.05f + cameraForward * 0.020f,
                hintStyle
            );

            renderUiOverlayText(
                *m_basicShader,
                *m_cubeMesh,
                overlayFrame,
                cameraLine,
                hudLeft + cameraRight * 0.52f - cameraUp * 0.11f + cameraForward * 0.020f,
                hintStyle
            );

            const glm::vec3 chargeTrackCenter =
                hudLeft + cameraRight * 0.35f - cameraUp * 0.37f + cameraForward * 0.020f;

            renderUiOverlayText(
                *m_basicShader,
                *m_cubeMesh,
                overlayFrame,
                "CHARGE",
                hudLeft - cameraUp * 0.37f + cameraForward * 0.020f,
                hintStyle
            );

            renderUiOverlayBox(
                *m_basicShader,
                *m_cubeMesh,
                overlayFrame,
                chargeTrackCenter,
                glm::vec3(chargeBarWidth, 0.026f, 0.012f),
                chargeTrackMaterial,
                glm::vec3(0.0f)
            );

            if (chargeFillWidth > 0.0f)
            {
                renderUiOverlayBox(
                    *m_basicShader,
                    *m_cubeMesh,
                    overlayFrame,
                    chargeTrackCenter - cameraRight * (0.5f * (chargeBarWidth - chargeFillWidth)),
                    glm::vec3(chargeFillWidth, 0.018f, 0.010f),
                    chargeFillMaterial,
                    glm::vec3(0.040f, 0.020f, 0.006f) * clampedCharge
                );
            }

            renderUiOverlayText(
                *m_basicShader,
                *m_cubeMesh,
                overlayFrame,
                "A/D AIM",
                hudLeft - cameraUp * 0.46f + cameraForward * 0.020f,
                hintStyle
            );

            renderUiOverlayText(
                *m_basicShader,
                *m_cubeMesh,
                overlayFrame,
                "SPACE SHOOT",
                hudLeft + cameraRight * 0.24f - cameraUp * 0.46f + cameraForward * 0.020f,
                hintStyle
            );

            renderUiOverlayText(
                *m_basicShader,
                *m_cubeMesh,
                overlayFrame,
                "ARROWS ENGLISH",
                hudLeft - cameraUp * 0.53f + cameraForward * 0.020f,
                hintStyle
            );

            renderUiOverlayText(
                *m_basicShader,
                *m_cubeMesh,
                overlayFrame,
                "TAB CYCLE 1 2 3 4",
                hudLeft + cameraRight * 0.24f - cameraUp * 0.53f + cameraForward * 0.020f,
                hintStyle
            );

            const char* cameraControlHint =
                (m_cameraRigState.mode == CameraViewMode::FreeLook)
                ? "J L ORBIT   I K TILT   U O ZOOM"
                : "ESC PAUSE   F2 QUALITY";

            renderUiOverlayText(
                *m_basicShader,
                *m_cubeMesh,
                overlayFrame,
                cameraControlHint,
                hudLeft - cameraUp * 0.60f + cameraForward * 0.020f,
                hintStyle
            );

            glEnable(GL_DEPTH_TEST);
        }
        else
        {
            const glm::vec3 menuBase =
                interpolatedCamera.position +
                cameraForward * 1.45f +
                cameraUp * 0.08f;

            const MaterialComponent backdropMaterial{
                .albedo = glm::vec3(0.06f, 0.045f, 0.04f),
                .specularStrength = 0.04f,
                .shininess = 8.0f,
                .surfaceType = MaterialSurfaceType::Generic,
                .roughness = 0.95f,
                .reflectivity = 0.02f,
                .clearcoatStrength = 0.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };

            const MaterialComponent cardMaterial{
                .albedo = glm::vec3(0.25f, 0.14f, 0.08f),
                .specularStrength = 0.10f,
                .shininess = 12.0f,
                .surfaceType = MaterialSurfaceType::Generic,
                .roughness = 0.70f,
                .reflectivity = 0.03f,
                .clearcoatStrength = 0.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };

            const MaterialComponent selectedCardMaterial{
                .albedo = glm::vec3(0.72f, 0.53f, 0.22f),
                .specularStrength = 0.18f,
                .shininess = 20.0f,
                .surfaceType = MaterialSurfaceType::Generic,
                .roughness = 0.38f,
                .reflectivity = 0.05f,
                .clearcoatStrength = 0.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };

            const MaterialComponent titleTextMaterial{
                .albedo = glm::vec3(0.95f, 0.90f, 0.76f),
                .specularStrength = 0.10f,
                .shininess = 12.0f,
                .surfaceType = MaterialSurfaceType::Generic,
                .roughness = 0.32f,
                .reflectivity = 0.04f,
                .clearcoatStrength = 0.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };

            const MaterialComponent bodyTextMaterial{
                .albedo = glm::vec3(0.95f, 0.91f, 0.82f),
                .specularStrength = 0.08f,
                .shininess = 10.0f,
                .surfaceType = MaterialSurfaceType::Generic,
                .roughness = 0.45f,
                .reflectivity = 0.03f,
                .clearcoatStrength = 0.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };

            const MaterialComponent selectedTextMaterial{
                .albedo = glm::vec3(0.17f, 0.10f, 0.04f),
                .specularStrength = 0.04f,
                .shininess = 8.0f,
                .surfaceType = MaterialSurfaceType::Generic,
                .roughness = 0.82f,
                .reflectivity = 0.02f,
                .clearcoatStrength = 0.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };

            const MaterialComponent hintTextMaterial{
                .albedo = glm::vec3(0.74f, 0.67f, 0.58f),
                .specularStrength = 0.04f,
                .shininess = 8.0f,
                .surfaceType = MaterialSurfaceType::Generic,
                .roughness = 0.88f,
                .reflectivity = 0.02f,
                .clearcoatStrength = 0.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };

            const UiTextStyle menuTitleStyle{
                .material = titleTextMaterial,
                .dynamicEmission = glm::vec3(0.035f, 0.020f, 0.008f),
                .cellSize = 0.0095f,
                .depth = 0.010f,
                .alignment = UiTextAlignment::Center
            };

            const UiTextStyle menuBodyStyle{
                .material = bodyTextMaterial,
                .dynamicEmission = glm::vec3(0.015f, 0.010f, 0.004f),
                .cellSize = 0.0100f,
                .depth = 0.010f,
                .alignment = UiTextAlignment::Center
            };

            const UiTextStyle selectedMenuStyle{
                .material = selectedTextMaterial,
                .dynamicEmission = glm::vec3(0.0f),
                .cellSize = 0.0100f,
                .depth = 0.010f,
                .alignment = UiTextAlignment::Center
            };

            const UiTextStyle hintStyle{
                .material = hintTextMaterial,
                .dynamicEmission = glm::vec3(0.010f, 0.008f, 0.004f),
                .cellSize = 0.0065f,
                .depth = 0.008f,
                .alignment = UiTextAlignment::Center
            };

            glDisable(GL_DEPTH_TEST);

            renderUiOverlayBox(
                *m_basicShader,
                *m_cubeMesh,
                overlayFrame,
                menuBase + cameraForward * 0.12f,
                glm::vec3(1.20f, 0.78f, 0.02f),
                backdropMaterial,
                glm::vec3(0.01f, 0.008f, 0.008f)
            );

            const glm::vec3 cardScale(0.72f, 0.08f, 0.035f);

            if (m_shellState == ApplicationShellState::MainMenu)
            {
                const MainMenuSelection entries[2] = {
                    MainMenuSelection::StartMatch,
                    MainMenuSelection::Quit
                };

                for (int i = 0; i < 2; ++i)
                {
                    const bool selected = entries[i] == m_mainMenuSelection;
                    const glm::vec3 cardCenter =
                        menuBase + cameraUp * (0.12f - 0.22f * static_cast<float>(i));

                    renderUiOverlayBox(
                        *m_basicShader,
                        *m_cubeMesh,
                        overlayFrame,
                        cardCenter,
                        cardScale,
                        selected ? selectedCardMaterial : cardMaterial,
                        selected ? glm::vec3(0.08f, 0.05f, 0.01f) : glm::vec3(0.0f)
                    );

                    renderUiOverlayText(
                        *m_basicShader,
                        *m_cubeMesh,
                        overlayFrame,
                        mainMenuSelectionLabel(entries[i]),
                        cardCenter + cameraForward * 0.020f,
                        selected ? selectedMenuStyle : menuBodyStyle
                    );
                }

                renderUiOverlayBox(
                    *m_basicShader,
                    *m_cubeMesh,
                    overlayFrame,
                    menuBase + cameraUp * 0.34f,
                    glm::vec3(0.52f, 0.07f, 0.025f),
                    backdropMaterial,
                    glm::vec3(0.03f, 0.02f, 0.01f)
                );

                renderUiOverlayText(
                    *m_basicShader,
                    *m_cubeMesh,
                    overlayFrame,
                    "MAIN MENU",
                    menuBase + cameraUp * 0.34f + cameraForward * 0.022f,
                    menuTitleStyle
                );

                renderUiOverlayText(
                    *m_basicShader,
                    *m_cubeMesh,
                    overlayFrame,
                    "UP/DOWN SELECT",
                    menuBase - cameraUp * 0.29f,
                    hintStyle
                );

                renderUiOverlayText(
                    *m_basicShader,
                    *m_cubeMesh,
                    overlayFrame,
                    "ENTER CONFIRM",
                    menuBase - cameraUp * 0.37f,
                    hintStyle
                );

                renderUiOverlayText(
                    *m_basicShader,
                    *m_cubeMesh,
                    overlayFrame,
                    "ESC QUIT",
                    menuBase - cameraUp * 0.45f,
                    hintStyle
                );
            }
            else if (m_shellState == ApplicationShellState::PauseMenu)
            {
                const PauseMenuSelection entries[3] = {
                    PauseMenuSelection::Resume,
                    PauseMenuSelection::RestartRack,
                    PauseMenuSelection::ReturnToMainMenu
                };

                for (int i = 0; i < 3; ++i)
                {
                    const bool selected = entries[i] == m_pauseMenuSelection;
                    const glm::vec3 cardCenter =
                        menuBase + cameraUp * (0.20f - 0.18f * static_cast<float>(i));

                    renderUiOverlayBox(
                        *m_basicShader,
                        *m_cubeMesh,
                        overlayFrame,
                        cardCenter,
                        cardScale,
                        selected ? selectedCardMaterial : cardMaterial,
                        selected ? glm::vec3(0.08f, 0.05f, 0.01f) : glm::vec3(0.0f)
                    );

                    renderUiOverlayText(
                        *m_basicShader,
                        *m_cubeMesh,
                        overlayFrame,
                        pauseMenuSelectionLabel(entries[i]),
                        cardCenter + cameraForward * 0.020f,
                        selected ? selectedMenuStyle : menuBodyStyle
                    );
                }

                renderUiOverlayBox(
                    *m_basicShader,
                    *m_cubeMesh,
                    overlayFrame,
                    menuBase + cameraUp * 0.40f,
                    glm::vec3(0.44f, 0.07f, 0.025f),
                    backdropMaterial,
                    glm::vec3(0.025f, 0.02f, 0.02f)
                );

                renderUiOverlayText(
                    *m_basicShader,
                    *m_cubeMesh,
                    overlayFrame,
                    "PAUSED",
                    menuBase + cameraUp * 0.40f + cameraForward * 0.022f,
                    menuTitleStyle
                );

                renderUiOverlayText(
                    *m_basicShader,
                    *m_cubeMesh,
                    overlayFrame,
                    "UP/DOWN SELECT",
                    menuBase - cameraUp * 0.29f,
                    hintStyle
                );

                renderUiOverlayText(
                    *m_basicShader,
                    *m_cubeMesh,
                    overlayFrame,
                    "ENTER CONFIRM",
                    menuBase - cameraUp * 0.37f,
                    hintStyle
                );

                renderUiOverlayText(
                    *m_basicShader,
                    *m_cubeMesh,
                    overlayFrame,
                    "ESC RESUME",
                    menuBase - cameraUp * 0.45f,
                    hintStyle
                );
            }

            glEnable(GL_DEPTH_TEST);
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
