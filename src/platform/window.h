#pragma once

struct GLFWwindow;

#include <string>

namespace BilliardsSaloon
{
    struct WindowDesc
    {
        int width {1600};
        int height {900};
        std::string title {"Billiards Saloon"};
    };

    class Window
    {
    public:
        explicit Window(const WindowDesc& desc);
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&) = delete;
        Window& operator=(Window&&) = delete;

        void pollEvents() const;
        void swapBuffers() const;

        [[nodiscard]] bool shouldClose() const;
        void requestClose() const;

        [[nodiscard]] int width() const;
        [[nodiscard]] int height() const;
        [[nodiscard]] float aspectRatio() const;

        [[nodiscard]] GLFWwindow* nativeHandle() const;

    private:
        static void framebufferSizeCallback(GLFWwindow* window, int width, int height);

        void initializeOpenGL();

        GLFWwindow* m_handle {nullptr};
        int m_width {0};
        int m_height {0};
    };
}