#pragma once
#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <functional>

namespace bs {

struct WindowConfig {
    int width  = 1280;
    int height = 720;
    std::string title  = "Billiards Saloon";
    bool vsync  = true;
};

class Window {
public:
    explicit Window(const WindowConfig& cfg); // constructor
    ~Window(); // destructor

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete; // Assignement operator for a window

    bool shouldClose() const;
    void swapBuffers();
    void pollEvents();
    GLFWwindow* handle() const { return m_handle; }

    // Always use this — not the logical window size (Retina has 2× pixels)
    void  framebufferSize(int& w, int& h) const;
    float aspectRatio() const;

    void setResizeCallback(std::function<void(int, int)> cb);

private:
    GLFWwindow* m_handle   = nullptr;
    std::function<void(int,int)> m_resizeCb; // resize callback function

    static void onFramebufferResize(GLFWwindow* handle, int w, int h);
};

} // namespace bs - contains "1 struct + 1 class"