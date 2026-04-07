#include "app/application.h"

#include "physics/billiards_physics.h"
#include "render/camera.h"
#include "scene/components.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <string>

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
    }

    Application::Application()
        : m_window(WindowDesc{})
    {
        m_basicShader = std::make_unique<Shader>(
            "assets/shaders/basic.vert",
            "assets/shaders/basic.frag"
        );

        m_cubeMesh = Mesh::createCube();
        m_planeMesh = Mesh::createPlane(2.84f, 1.42f);

        constexpr float BALL_RADIUS = 0.028575f;
        m_sphereMesh = Mesh::createUVSphere(BALL_RADIUS, 32U, 16U);

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
                "Table Surface",
                tableTransform,
                StaticMeshComponent{MeshPrimitive::Plane},
                MaterialComponent{
                    .albedo = glm::vec3(0.10f, 0.42f, 0.16f),
                    .specularStrength = 0.08f,
                    .shininess = 8.0f
                }
            );

            m_registry.emplace<TableBoundsComponent>(table, TableBoundsComponent{
                .halfWidth = 1.42f,
                .halfDepth = 0.71f,
                .railRestitution = 0.92f,
                .ballRestitution = 0.96f,
                .rollingFrictionCoefficient = 0.020f,
                .stopSpeedThreshold = 0.02f
            });
        }

        {
            TransformComponent cueBallTransform;
            cueBallTransform.position = glm::vec3(0.0f, BALL_RADIUS, 0.42f);
            cueBallTransform.previousPosition = cueBallTransform.position;
            cueBallTransform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            cueBallTransform.previousRotation = cueBallTransform.rotation;
            cueBallTransform.scale = glm::vec3(1.0f);
            cueBallTransform.previousScale = cueBallTransform.scale;

            const Entity cueBall = createSceneEntity(
                m_registry,
                "Cue Ball",
                cueBallTransform,
                StaticMeshComponent{MeshPrimitive::Sphere},
                MaterialComponent{
                    .albedo = glm::vec3(0.93f, 0.93f, 0.91f),
                    .specularStrength = 0.95f,
                    .shininess = 128.0f
                }
            );

            m_registry.emplace<BallComponent>(cueBall, BallComponent{
                .radius = BALL_RADIUS,
                .massKg = 0.17f,
                .linearVelocity = glm::vec3(0.0f),
                .angularVelocity = glm::vec3(0.0f),
                .isCueBall = true
            });
        }

        {
            TransformComponent eightBallTransform;
            eightBallTransform.position = glm::vec3(0.0f, BALL_RADIUS, -0.16f);
            eightBallTransform.previousPosition = eightBallTransform.position;
            eightBallTransform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            eightBallTransform.previousRotation = eightBallTransform.rotation;
            eightBallTransform.scale = glm::vec3(1.0f);
            eightBallTransform.previousScale = eightBallTransform.scale;

            const Entity ball = createSceneEntity(
                m_registry,
                "Eight Ball",
                eightBallTransform,
                StaticMeshComponent{MeshPrimitive::Sphere},
                MaterialComponent{
                    .albedo = glm::vec3(0.05f, 0.05f, 0.06f),
                    .specularStrength = 0.90f,
                    .shininess = 128.0f
                }
            );

            m_registry.emplace<BallComponent>(ball, BallComponent{
                .radius = BALL_RADIUS,
                .massKg = 0.17f,
                .linearVelocity = glm::vec3(0.0f),
                .angularVelocity = glm::vec3(0.0f),
                .isCueBall = false
            });
        }

        {
            TransformComponent redBallTransform;
            redBallTransform.position = glm::vec3(0.06f, BALL_RADIUS, -0.23f);
            redBallTransform.previousPosition = redBallTransform.position;
            redBallTransform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            redBallTransform.previousRotation = redBallTransform.rotation;
            redBallTransform.scale = glm::vec3(1.0f);
            redBallTransform.previousScale = redBallTransform.scale;

            const Entity ball = createSceneEntity(
                m_registry,
                "Red Ball",
                redBallTransform,
                StaticMeshComponent{MeshPrimitive::Sphere},
                MaterialComponent{
                    .albedo = glm::vec3(0.73f, 0.10f, 0.08f),
                    .specularStrength = 0.92f,
                    .shininess = 128.0f
                }
            );

            m_registry.emplace<BallComponent>(ball, BallComponent{
                .radius = BALL_RADIUS,
                .massKg = 0.17f,
                .linearVelocity = glm::vec3(0.0f),
                .angularVelocity = glm::vec3(0.0f),
                .isCueBall = false
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