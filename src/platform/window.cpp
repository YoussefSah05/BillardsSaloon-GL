#include "platform/window.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    // GLFW calls this from C code, so it must not throw. Fatal failures are
    // still reported through the return values checked below.
    void glfwErrorCallback(int errorCode, const char* description)
    {
        std::cerr << "GLFW error (" << errorCode << "): " << description << '\n';
    }

    // The monitor that contains the centre of the window, or the primary one.
    GLFWmonitor* monitorForWindow(GLFWwindow* window)
    {
        int windowX = 0;
        int windowY = 0;
        int windowWidth = 0;
        int windowHeight = 0;
        glfwGetWindowPos(window, &windowX, &windowY);
        glfwGetWindowSize(window, &windowWidth, &windowHeight);

        const int centerX = windowX + windowWidth / 2;
        const int centerY = windowY + windowHeight / 2;

        int monitorCount = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

        for (int i = 0; i < monitorCount; ++i)
        {
            int x = 0;
            int y = 0;
            int width = 0;
            int height = 0;
            glfwGetMonitorWorkarea(monitors[i], &x, &y, &width, &height);

            if ((centerX >= x) && (centerX < x + width) && (centerY >= y) && (centerY < y + height))
            {
                return monitors[i];
            }
        }

        return glfwGetPrimaryMonitor();
    }
}

namespace BilliardsSaloon
{
    Window::Window(const WindowDesc& desc)
    {
        glfwSetErrorCallback(glfwErrorCallback);

        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW.");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        glfwWindowHint(GLFW_SAMPLES, 4);

    #if defined(BS_DEBUG)
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    #endif

        // Never open a window larger than the screen it lands on.
        int width = desc.width;
        int height = desc.height;
        if (GLFWmonitor* primary = glfwGetPrimaryMonitor())
        {
            int x = 0;
            int y = 0;
            int workWidth = 0;
            int workHeight = 0;
            glfwGetMonitorWorkarea(primary, &x, &y, &workWidth, &workHeight);

            if ((workWidth > 0) && (workHeight > 0))
            {
                width = std::min(width, workWidth * 9 / 10);
                height = std::min(height, workHeight * 9 / 10);
            }
        }

        m_handle = glfwCreateWindow(width, height, desc.title.c_str(), nullptr, nullptr);
        if (m_handle == nullptr)
        {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window.");
        }

        glfwMakeContextCurrent(m_handle);

        // VSync on: an uncapped frame rate only burns GPU time, and physics
        // already runs at its own fixed rate. A settings toggle comes later.
        glfwSwapInterval(1);

        glfwSetWindowUserPointer(m_handle, this);
        glfwSetFramebufferSizeCallback(m_handle, &Window::framebufferSizeCallback);
        glfwSetScrollCallback(m_handle, &Window::scrollCallback);

        initializeOpenGL();

        // On HiDPI displays the framebuffer is larger than the window size
        // requested above; size the viewport from the real framebuffer.
        refreshFramebufferSize();

        if (desc.fullscreen)
        {
            setFullscreen(true);
        }
    }

    Window::~Window()
    {
        if (m_handle != nullptr)
        {
            glfwDestroyWindow(m_handle);
            m_handle = nullptr;
        }

        glfwTerminate();
    }

    void Window::pollEvents() const
    {
        glfwPollEvents();
    }

    void Window::swapBuffers() const
    {
        glfwSwapBuffers(m_handle);
    }

    void Window::setTitle(const std::string& title) const
    {
        glfwSetWindowTitle(m_handle, title.c_str());
    }

    bool Window::shouldClose() const
    {
        return glfwWindowShouldClose(m_handle) == GLFW_TRUE;
    }

    void Window::requestClose() const
    {
        glfwSetWindowShouldClose(m_handle, GLFW_TRUE);
    }

    int Window::width() const
    {
        return m_width;
    }

    int Window::height() const
    {
        return m_height;
    }

    float Window::aspectRatio() const
    {
        return (m_height > 0) ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
    }

    glm::vec2 Window::windowSize() const
    {
        int width = 0;
        int height = 0;
        glfwGetWindowSize(m_handle, &width, &height);
        return glm::vec2(static_cast<float>(std::max(width, 1)), static_cast<float>(std::max(height, 1)));
    }

    void Window::setFullscreen(bool fullscreen)
    {
        if (fullscreen == m_fullscreen)
        {
            return;
        }

        if (fullscreen)
        {
            GLFWmonitor* monitor = monitorForWindow(m_handle);
            const GLFWvidmode* mode = (monitor != nullptr) ? glfwGetVideoMode(monitor) : nullptr;
            if (mode == nullptr)
            {
                return;
            }

            glfwGetWindowPos(m_handle, &m_windowedX, &m_windowedY);
            glfwGetWindowSize(m_handle, &m_windowedWidth, &m_windowedHeight);

            // Using the monitor's current mode avoids a display mode switch.
            glfwSetWindowMonitor(m_handle, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        }
        else
        {
            glfwSetWindowMonitor(
                m_handle,
                nullptr,
                m_windowedX,
                m_windowedY,
                m_windowedWidth,
                m_windowedHeight,
                GLFW_DONT_CARE
            );
        }

        m_fullscreen = fullscreen;

        // Changing the monitor resets the swap interval on some platforms.
        glfwSwapInterval(1);
        refreshFramebufferSize();
    }

    void Window::toggleFullscreen()
    {
        setFullscreen(!m_fullscreen);
    }

    bool Window::isFullscreen() const
    {
        return m_fullscreen;
    }

    bool Window::isFocused() const
    {
        return glfwGetWindowAttrib(m_handle, GLFW_FOCUSED) == GLFW_TRUE;
    }

    void Window::setCursorCaptured(bool captured)
    {
        if (captured == m_cursorCaptured)
        {
            return;
        }

        glfwSetInputMode(m_handle, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

        if (glfwRawMouseMotionSupported() == GLFW_TRUE)
        {
            glfwSetInputMode(m_handle, GLFW_RAW_MOUSE_MOTION, captured ? GLFW_TRUE : GLFW_FALSE);
        }

        m_cursorCaptured = captured;
    }

    bool Window::isCursorCaptured() const
    {
        return m_cursorCaptured;
    }

    float Window::consumeScrollDelta()
    {
        const float delta = m_scrollDelta;
        m_scrollDelta = 0.0f;
        return delta;
    }

    GLFWwindow* Window::nativeHandle() const
    {
        return m_handle;
    }

    void Window::refreshFramebufferSize()
    {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(m_handle, &width, &height);

        m_width = std::max(width, 1);
        m_height = std::max(height, 1);
        glViewport(0, 0, m_width, m_height);
    }

    void Window::framebufferSizeCallback(GLFWwindow* window, int width, int height)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self == nullptr)
        {
            return;
        }

        self->m_width = std::max(width, 1);
        self->m_height = std::max(height, 1);

        glViewport(0, 0, self->m_width, self->m_height);
    }

    void Window::scrollCallback(GLFWwindow* window, double, double yOffset)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self != nullptr)
        {
            self->m_scrollDelta += static_cast<float>(yOffset);
        }
    }

    void Window::initializeOpenGL()
    {
        const int version = gladLoadGL(glfwGetProcAddress);
        if (version == 0)
        {
            throw std::runtime_error("Failed to load OpenGL functions via GLAD.");
        }

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_MULTISAMPLE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }
}
