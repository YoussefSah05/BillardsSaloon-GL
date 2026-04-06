#include "platform/window.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace
{
    void glfwErrorCallback(int errorCode, const char* description)
    {
        throw std::runtime_error(
            "GLFW error (" + std::to_string(errorCode) + "): " + std::string(description)
        );
    }
}

namespace BilliardsSaloon
{
    Window::Window(const WindowDesc& desc)
        : m_width(desc.width), m_height(desc.height)
    {
        glfwSetErrorCallback(glfwErrorCallback);

        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW.");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    #if defined(BS_DEBUG)
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    #endif

        m_handle = glfwCreateWindow(m_width, m_height, desc.title.c_str(), nullptr, nullptr);
        if (m_handle == nullptr)
        {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window.");
        }

        glfwMakeContextCurrent(m_handle);

        // Disable VSync initially so render rate is not forced to 60 Hz.
        // Physics will be fixed-rate independently.
        glfwSwapInterval(0);

        glfwSetWindowUserPointer(m_handle, this);
        glfwSetFramebufferSizeCallback(m_handle, &Window::framebufferSizeCallback);

        initializeOpenGL();

        glViewport(0, 0, m_width, m_height);
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

    bool Window::shouldClose() const
    {
        return glfwWindowShouldClose(m_handle) == GLFW_TRUE; // to return the boolean since glfwfunction returns int
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

    GLFWwindow* Window::nativeHandle() const
    {
        return m_handle;
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

    void Window::initializeOpenGL()
    {
        const int version = gladLoadGL(glfwGetProcAddress);
        if (version == 0)
        {
            throw std::runtime_error("Failed to load OpenGL functions via GLAD.");
        }

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }
}