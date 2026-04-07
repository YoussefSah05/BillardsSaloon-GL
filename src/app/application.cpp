#include "app/application.h"

#include "physics/billiards_physics.h"
#include "render/camera.h"
#include "scene/components.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace BilliardsSaloon
{
    namespace
    {
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

        glm::vec3 aimDirectionFromAngle(float angleRadians)
        {
            return glm::normalize(glm::vec3(std::sin(angleRadians), 0.0f, -std::cos(angleRadians)));
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
    }

    Application::Application()
        : m_window(WindowDesc{})
    {
        m_variant = &eightBallVariant();

        m_matchState.discipline = m_variant->discipline;
        m_matchState.flowPhase = MatchFlowPhase::BreakShot;
        m_matchState.activePlayerIndex = 0;
        m_matchState.shotInProgress = false;
        m_matchState.foulCommittedThisTurn = false;
        m_matchState.ballInHand = false;

        m_basicShader = std::make_unique<Shader>(
            "assets/shaders/basic.vert",
            "assets/shaders/basic.frag"
        );

        m_cubeMesh = Mesh::createCube();
        m_planeMesh = Mesh::createPlane(m_variant->table.clothWidth, m_variant->table.clothDepth);
        m_sphereMesh = Mesh::createUVSphere(m_variant->table.ballRadius, 32U, 16U);

        {
            TransformComponent tableTransform;
            tableTransform.position = glm::vec3(0.0f, 0.0f, 0.0f);
            tableTransform.previousPosition = tableTransform.position;
            tableTransform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            tableTransform.previousRotation = tableTransform.rotation;
            tableTransform.scale = glm::vec3(1.0f);
            tableTransform.previousScale = tableTransform.scale;

            const Entity table = createSceneEntity(
                m_registry,
                m_variant->displayName + " Table",
                tableTransform,
                StaticMeshComponent{MeshPrimitive::Plane},
                MaterialComponent{
                    .albedo = glm::vec3(0.10f, 0.42f, 0.16f),
                    .specularStrength = 0.08f,
                    .shininess = 8.0f
                }
            );

            m_registry.emplace<TableBoundsComponent>(table, TableBoundsComponent{
                .halfWidth = 0.5f * m_variant->table.clothWidth,
                .halfDepth = 0.5f * m_variant->table.clothDepth,
                .railRestitution = 0.92f,
                .ballRestitution = 0.96f,
                .rollingFrictionCoefficient = 0.020f,
                .stopSpeedThreshold = 0.02f
            });
        }

        {
            TransformComponent cueBallTransform;
            cueBallTransform.position = glm::vec3(0.0f, m_variant->table.ballRadius, 0.42f);
            cueBallTransform.previousPosition = cueBallTransform.position;
            cueBallTransform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            cueBallTransform.previousRotation = cueBallTransform.rotation;
            cueBallTransform.scale = glm::vec3(1.0f);
            cueBallTransform.previousScale = cueBallTransform.scale;

            const Entity cueBall = createSceneEntity(
                m_registry,
                m_variant->cueBall.name,
                cueBallTransform,
                StaticMeshComponent{MeshPrimitive::Sphere},
                MaterialComponent{
                    .albedo = m_variant->cueBall.albedo,
                    .specularStrength = m_variant->cueBall.specularStrength,
                    .shininess = m_variant->cueBall.shininess
                }
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

            TransformComponent transform;
            transform.position = rackPositions[i];
            transform.previousPosition = transform.position;
            transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            transform.previousRotation = transform.rotation;
            transform.scale = glm::vec3(1.0f);
            transform.previousScale = transform.scale;

            const Entity ball = createSceneEntity(
                m_registry,
                definition.name,
                transform,
                StaticMeshComponent{MeshPrimitive::Sphere},
                MaterialComponent{
                    .albedo = definition.albedo,
                    .specularStrength = definition.specularStrength,
                    .shininess = definition.shininess
                }
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

        {
            TransformComponent cameraTransform;
            cameraTransform.position = glm::vec3(0.0f, 1.10f, 1.95f);
            cameraTransform.previousPosition = cameraTransform.position;
            cameraTransform.rotation = glm::quat(glm::vec3(glm::radians(-28.0f), 0.0f, 0.0f));
            cameraTransform.previousRotation = cameraTransform.rotation;
            cameraTransform.scale = glm::vec3(1.0f);
            cameraTransform.previousScale = cameraTransform.scale;

            m_registry.emplace<TransformComponent>(m_cameraEntity, cameraTransform);
        }

        m_registry.emplace<CameraComponent>(m_cameraEntity, CameraComponent{
            .verticalFieldOfViewRadians = glm::radians(55.0f),
            .nearPlane = 0.05f,
            .farPlane = 50.0f
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

        if (glfwGetKey(handle, GLFW_KEY_R) == GLFW_PRESS)
        {
            resetCueBall();
            m_shotState.phase = ShotPhase::Aiming;
            m_shotState.charge01 = 0.0f;
            m_matchState.shotInProgress = false;
        }

        if (m_shotState.phase == ShotPhase::BallsInMotion)
        {
            m_spaceWasDownLastFrame = glfwGetKey(handle, GLFW_KEY_SPACE) == GLFW_PRESS;
            return;
        }

        constexpr float AIM_SPEED = 1.8f;

        if (glfwGetKey(handle, GLFW_KEY_A) == GLFW_PRESS)
        {
            m_shotState.aimAngleRadians += static_cast<float>(FIXED_TIME_STEP) * AIM_SPEED;
        }

        if (glfwGetKey(handle, GLFW_KEY_D) == GLFW_PRESS)
        {
            m_shotState.aimAngleRadians -= static_cast<float>(FIXED_TIME_STEP) * AIM_SPEED;
        }

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
                fireCurrentShot();
                m_shotState.phase = ShotPhase::BallsInMotion;
                m_shotState.charge01 = 0.0f;
                m_matchState.shotInProgress = true;
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
    }

    void Application::fireCurrentShot()
    {
        const Entity cueBall = findCueBall();
        if (!cueBall.isValid())
        {
            return;
        }

        BallComponent* ball = m_registry.tryGet<BallComponent>(cueBall);
        if (ball == nullptr)
        {
            return;
        }

        const glm::vec3 aimDirection = aimDirectionFromAngle(m_shotState.aimAngleRadians);

        constexpr float MIN_SHOT_SPEED = 0.4f;
        constexpr float MAX_SHOT_SPEED = 3.8f;

        const float shotSpeed =
            MIN_SHOT_SPEED + (MAX_SHOT_SPEED - MIN_SHOT_SPEED) * m_shotState.charge01;

        ball->linearVelocity = aimDirection * shotSpeed;
        ball->angularVelocity = glm::vec3(0.0f);
    }

    void Application::updateFixed(double deltaTimeSeconds)
    {
        Physics::stepBilliardsWorld(m_registry, deltaTimeSeconds);

        if ((m_shotState.phase == ShotPhase::BallsInMotion) && !Physics::anyBallInMotion(m_registry))
        {
            m_shotState.phase = ShotPhase::Aiming;
            m_matchState.shotInProgress = false;

            if (m_matchState.flowPhase == MatchFlowPhase::BreakShot)
            {
                m_matchState.flowPhase = MatchFlowPhase::TableOpen;
            }
        }
    }

    void Application::render(double alpha)
    {
        glViewport(0, 0, m_window.width(), m_window.height());

        glClearColor(0.05f, 0.035f, 0.025f, 1.0f);
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

        m_basicShader->bind();
        m_basicShader->setMat4("uView", view);
        m_basicShader->setMat4("uProjection", projection);
        m_basicShader->setVec3("uViewPosition", interpolatedCamera.position);

        m_basicShader->setVec3(
            "uDirectionalLightDirection",
            glm::normalize(glm::vec3(-0.6f, -1.0f, -0.25f))
        );
        m_basicShader->setVec3(
            "uDirectionalLightColor",
            glm::vec3(0.65f, 0.62f, 0.58f)
        );

        m_basicShader->setVec3(
            "uPointLightPosition",
            glm::vec3(0.55f, 1.15f, 0.35f)
        );
        m_basicShader->setVec3(
            "uPointLightColor",
            glm::vec3(1.00f, 0.72f, 0.42f)
        );

        const Entity cueBall = findCueBall();
        glm::vec3 cueBallPosition(0.0f);

        if (cueBall.isValid())
        {
            if (const TransformComponent* cueBallTransform = m_registry.tryGet<TransformComponent>(cueBall))
            {
                cueBallPosition =
                    interpolateTransform(*cueBallTransform, static_cast<float>(alpha)).position;
            }
        }

        m_registry.view<TransformComponent, StaticMeshComponent, MaterialComponent>().each(
            [&](Entity entity, TransformComponent& transform, StaticMeshComponent& meshComponent, MaterialComponent& material)
            {
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

                m_basicShader->setMat4("uModel", model);
                m_basicShader->setVec3("uMaterialAlbedo", material.albedo);
                m_basicShader->setFloat("uMaterialSpecularStrength", material.specularStrength);
                m_basicShader->setFloat("uMaterialShininess", material.shininess);

                const bool highlightCueBall =
                    (entity == cueBall) && (m_shotState.phase != ShotPhase::BallsInMotion);

                m_basicShader->setInt("uUseEmission", highlightCueBall ? 1 : 0);

                if (highlightCueBall)
                {
                    const float glow = 0.08f + 0.25f * m_shotState.charge01;
                    m_basicShader->setVec3("uEmissionColor", glm::vec3(glow, glow, glow * 0.85f));
                }
                else
                {
                    m_basicShader->setVec3("uEmissionColor", glm::vec3(0.0f));
                }

                mesh->draw();
            }
        );

        if ((m_shotState.phase != ShotPhase::BallsInMotion) && cueBall.isValid())
        {
            const glm::vec3 aimDirection = aimDirectionFromAngle(m_shotState.aimAngleRadians);

            TransformComponent guideTransform;
            guideTransform.position = cueBallPosition + aimDirection * 0.18f;
            guideTransform.previousPosition = guideTransform.position;
            guideTransform.rotation = glm::quat(glm::vec3(0.0f, -m_shotState.aimAngleRadians, 0.0f));
            guideTransform.previousRotation = guideTransform.rotation;
            guideTransform.scale = glm::vec3(0.03f, 0.03f, 0.18f + 0.35f * m_shotState.charge01);
            guideTransform.previousScale = guideTransform.scale;

            m_basicShader->setMat4("uModel", composeMatrix(
                guideTransform.position,
                guideTransform.rotation,
                guideTransform.scale
            ));
            m_basicShader->setVec3("uMaterialAlbedo", glm::vec3(0.92f, 0.82f, 0.42f));
            m_basicShader->setFloat("uMaterialSpecularStrength", 0.15f);
            m_basicShader->setFloat("uMaterialShininess", 8.0f);
            m_basicShader->setInt("uUseEmission", 1);
            m_basicShader->setVec3(
                "uEmissionColor",
                glm::vec3(0.10f, 0.08f, 0.02f) + glm::vec3(0.10f, 0.06f, 0.01f) * m_shotState.charge01
            );

            m_cubeMesh->draw();

            m_basicShader->setInt("uUseEmission", 0);
            m_basicShader->setVec3("uEmissionColor", glm::vec3(0.0f));
        }
    }
}