#include "app/application.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>

namespace BilliardsSaloon
{
    Application::Application()
        : m_window(WindowDesc{})
    {
    }

    int Application::run()
    {
        m_timer.reset();

        while (!m_window.shouldClose())
        {
            m_window.pollEvents();

            processPlatformInput();

            double frameTime = m_timer.tick();

            // Prevent the "spiral of death" after breakpoints, hitches, or window drags.
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
        (void)deltaTimeSeconds;

        // This is where all deterministic simulation will live:
        // - billiards physics
        // - rules
        // - AI
        // - server-authoritative shot resolution
        // - more & more
    }

    void Application::render(double alpha)
    {
        (void)alpha;

        glViewport(0, 0, m_window.width(), m_window.height());

        glClearColor(0.08f, 0.06f, 0.04f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Later:
        // - interpolate previous/current transforms using alpha
        // - geometry pass
        // - lighting pass
        // - post-processing
        // - more&more
    }
}