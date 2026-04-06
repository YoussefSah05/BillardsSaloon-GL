#include "app/application.h"

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
        m_demoBall = m_registry.createEntity();

        m_registry.emplace<NameComponent>(m_demoBall, NameComponent{"Demo Ball"});
        m_registry.emplace<TransformComponent>(m_demoBall, TransformComponent{});
        m_registry.emplace<SpinComponent>(m_demoBall, SpinComponent{
            .axis = glm::vec3(0.0f, 1.0f, 0.0f),
            .radiansPerSecond = 1.35f
        });
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

        float visualPhase = 0.0f;

        if (const TransformComponent* transform = m_registry.tryGet<TransformComponent>(m_demoBall))
        {
            const InterpolatedTransform interpolated =
                interpolateTransform(*transform, static_cast<float>(alpha));

            const glm::vec3 rightVector = interpolated.rotation * glm::vec3(1.0f, 0.0f, 0.0f);

            visualPhase = 0.5f * (rightVector.x + 1.0f);
        }

        const float red   = 0.08f + 0.03f * visualPhase;
        const float green = 0.06f + 0.08f * visualPhase;
        const float blue  = 0.04f + 0.02f * visualPhase;

        glClearColor(red, green, blue, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
}