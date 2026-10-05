#pragma once

struct GLFWwindow;

#include <glm/glm.hpp>

#include <string>

namespace BilliardsSaloon
{
    // Receives raw window events in the order they happen (used by the UI,
    // which needs text input and precise click timing that polling loses).
    class WindowEventSink
    {
    public:
        virtual ~WindowEventSink() = default;

        virtual void onKey(int /*key*/, int /*action*/, int /*mods*/) {}
        virtual void onChar(unsigned int /*codepoint*/) {}
        virtual void onCursorPos(double /*x*/, double /*y*/, int /*mods*/) {}
        virtual void onCursorEnter(bool /*entered*/) {}
        virtual void onMouseButton(int /*button*/, int /*action*/, int /*mods*/) {}
        virtual void onScroll(double /*yOffset*/, int /*mods*/) {}
        virtual void onFramebufferSize(int /*width*/, int /*height*/) {}
        virtual void onContentScale(float /*scale*/) {}
    };

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

        // Ratio of framebuffer pixels to screen coordinates (2 on Retina).
        [[nodiscard]] float contentScale() const;

        // At most one sink; pass nullptr to detach. The sink must outlive the window or be detached.
        void setEventSink(WindowEventSink* sink);

        [[nodiscard]] GLFWwindow* nativeHandle() const;

    private:
        static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
        static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);
        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void charCallback(GLFWwindow* window, unsigned int codepoint);
        static void cursorPosCallback(GLFWwindow* window, double x, double y);
        static void cursorEnterCallback(GLFWwindow* window, int entered);
        static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void contentScaleCallback(GLFWwindow* window, float xScale, float yScale);

        void initializeOpenGL();
        void refreshFramebufferSize();

        GLFWwindow* m_handle {nullptr};
        int m_width {0};
        int m_height {0};

        bool m_fullscreen {false};
        bool m_cursorCaptured {false};
        float m_scrollDelta {0.0f};
        WindowEventSink* m_sink {nullptr};
        int m_currentMods {0};

        // Window placement to restore when leaving fullscreen.
        int m_windowedX {0};
        int m_windowedY {0};
        int m_windowedWidth {0};
        int m_windowedHeight {0};
    };
}
