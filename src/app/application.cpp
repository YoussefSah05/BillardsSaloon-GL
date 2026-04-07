#include "app/application.h"

#include "render/camera.h"
#include "scene/components.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

namespace BilliardsSaloon
{
    Application::Application()
        : m_window(WindowDesc{})
    {
        m_basicShader = std::make_unique<Shader>(
            "../assets/shaders/basic.vert",
            "../assets/shaders/basic.frag"
        );

        m_cubeMesh = Mesh::createCube();

        m_demoCube = m_registry.createEntity();
        m_registry.emplace<NameComponent>(m_demoCube, NameComponent{"Demo Cube"});

        {
            TransformComponent transform;
            transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
            transform.previousPosition = transform.position;
            transform.scale = glm::vec3(1.0f);
            transform.previousScale = transform.scale;
            transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            transform.previousRotation = transform.rotation;

            m_registry.emplace<TransformComponent>(m_demoCube, transform);
        }

        m_registry.emplace<SpinComponent>(m_demoCube, SpinComponent{
            .axis = glm::vec3(0.3f, 1.0f, 0.2f),
            .radiansPerSecond = 1.2f
        });
        m_registry.emplace<MeshRenderComponent>(m_demoCube, MeshRenderComponent{});

        m_cameraEntity = m_registry.createEntity();
        m_registry.emplace<NameComponent>(m_cameraEntity, NameComponent{"Main Camera"});

        {
            TransformComponent cameraTransform;
            cameraTransform.position = glm::vec3(0.0f, 1.5f, 4.0f);
            cameraTransform.previousPosition = cameraTransform.position;
            cameraTransform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            cameraTransform.previousRotation = cameraTransform.rotation;
            cameraTransform.scale = glm::vec3(1.0f);
            cameraTransform.previousScale = cameraTransform.scale;

            m_registry.emplace<TransformComponent>(m_cameraEntity, cameraTransform);
        }

        m_registry.emplace<CameraComponent>(m_cameraEntity, CameraComponent{});
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
    }

    void Application::updateFixed(double deltaTimeSeconds)
    {
        m_registry.view<TransformComponent, SpinComponent>().each(
            [deltaTimeSeconds](Entity, TransformComponent& transform, SpinComponent& spin)
            {
                transform.syncPrevious();

                glm::vec3 axis = spin.axis;
                const float axisLengthSquared = glm::dot(axis, axis);

                if (axisLengthSquared < 1.0e-6f)
                {
                    axis = glm::vec3(0.0f, 1.0f, 0.0f);
                }
                else
                {
                    axis = glm::normalize(axis);
                }

                const float deltaAngle =
                    static_cast<float>(deltaTimeSeconds) * spin.radiansPerSecond;

                const glm::quat deltaRotation = glm::angleAxis(deltaAngle, axis);
                transform.rotation = glm::normalize(deltaRotation * transform.rotation);
            }
        );
    }

    void Application::render(double alpha)
    {
        glViewport(0, 0, m_window.width(), m_window.height());

        glClearColor(0.08f, 0.06f, 0.04f, 1.0f);
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
        m_basicShader->setVec3("uLightDirection", glm::normalize(glm::vec3(-0.6f, -1.0f, -0.4f)));
        m_basicShader->setVec3("uAlbedo", glm::vec3(0.82f, 0.74f, 0.62f));

        m_registry.view<TransformComponent, MeshRenderComponent>().each(
            [&](Entity, TransformComponent& transform, MeshRenderComponent&)
            {
                const glm::mat4 model =
                    composeInterpolatedMatrix(transform, static_cast<float>(alpha));

                m_basicShader->setMat4("uModel", model);
                m_cubeMesh->draw();
            }
        );
    }
}