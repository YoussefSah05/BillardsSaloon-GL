#pragma once

struct GLFWwindow;

#include <glm/glm.hpp>

#include <string>

namespace BilliardsSaloon
{
    struct WindowDesc
    {
        int width {1600};
        int height {900};
        std::string title {"Billiards Saloon"};
        bool fullscreen {false};
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
        void setTitle(const std::string& title) const;

        [[nodiscard]] bool shouldClose() const;
        void requestClose() const;

        // Framebuffer size in pixels (larger than the window size on HiDPI displays).
        [[nodiscard]] int width() const;
        [[nodiscard]] int height() const;
        [[nodiscard]] float aspectRatio() const;

        // Window size in screen coordinates, the space cursor positions use.
        [[nodiscard]] glm::vec2 windowSize() const;

        // Switches between a window and fullscreen on the monitor the window is on.
        void setFullscreen(bool fullscreen);
        void toggleFullscreen();
        [[nodiscard]] bool isFullscreen() const;

        [[nodiscard]] bool isFocused() const;

        // Hides the cursor and reports unbounded relative motion (for aiming).
        void setCursorCaptured(bool captured);
        [[nodiscard]] bool isCursorCaptured() const;

        // Scroll wheel movement since the last call, in wheel steps.
        [[nodiscard]] float consumeScrollDelta();

        [[nodiscard]] GLFWwindow* nativeHandle() const;

    private:
        static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
        static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);

        void initializeOpenGL();
        void refreshFramebufferSize();

        GLFWwindow* m_handle {nullptr};
        int m_width {0};
        int m_height {0};

        bool m_fullscreen {false};
        bool m_cursorCaptured {false};
        float m_scrollDelta {0.0f};

        // Window placement to restore when leaving fullscreen.
        int m_windowedX {0};
        int m_windowedY {0};
        int m_windowedWidth {0};
        int m_windowedHeight {0};
    };
}
