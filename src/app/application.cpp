#include "app/application.h"

#include "gameplay/turn_rules.h"
#include "physics/billiards_physics.h"
#include "render/camera.h"
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

        struct PointLightRig
        {
            std::array<glm::vec3, POINT_LIGHT_COUNT> positions {};
            std::array<glm::vec3, POINT_LIGHT_COUNT> colors {};
        };

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

        m_matchState.discipline = m_variant->discipline;
        m_matchState.flowPhase = MatchFlowPhase::BreakShot;
        m_matchState.activePlayerIndex = 0;
        m_matchState.winnerPlayerIndex = -1;
        m_matchState.shotInProgress = false;
        m_matchState.foulCommittedThisTurn = false;
        m_matchState.ballInHand = false;

        m_basicShader = std::make_unique<Shader>(
            "../assets/shaders/basic.vert",
            "../assets/shaders/basic.frag"
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

            while (m_accumulator >= FIXED_TIME_STEP)
            {
                updateFixed(FIXED_TIME_STEP);
                m_accumulator -= FIXED_TIME_STEP;
                m_simulationTime += FIXED_TIME_STEP;
                ++m_fixedFrameIndex;
            }

            const double alpha = m_accumulator / FIXED_TIME_STEP;
            render(alpha);

            m_window.swapBuffers();
        }

        return 0;
    }

    void Application::processPlatformInput()
    {
        GLFWwindow* handle = m_window.nativeHandle();

        if (glfwGetKey(handle, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            m_window.requestClose();
        }

        if (m_matchState.flowPhase == MatchFlowPhase::FrameOver)
        {
            return;
        }

        if (glfwGetKey(handle, GLFW_KEY_R) == GLFW_PRESS)
        {
            resetCueBall();
            m_shotState.phase = ShotPhase::Aiming;
            m_shotState.charge01 = 0.0f;
            m_matchState.shotInProgress = false;
            m_currentShotResult.clear();
        }

        if (m_shotState.phase == ShotPhase::BallsInMotion)
        {
            m_spaceWasDownLastFrame = glfwGetKey(handle, GLFW_KEY_SPACE) == GLFW_PRESS;
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
    }

    Entity Application::findCueBall() const
    {
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
    }

    void Application::render(double alpha)
    {
        glViewport(0, 0, m_window.width(), m_window.height());

        if (m_matchState.flowPhase == MatchFlowPhase::FrameOver)
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

        if ((m_shotState.phase != ShotPhase::BallsInMotion) &&
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
    }
}
