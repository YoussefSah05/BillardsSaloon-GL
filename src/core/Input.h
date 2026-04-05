#pragma once
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <unordered_map>

namespace bs {

// Stateless per-system queries — call Input::beginFrame() at the top of each frame.
// No system needs to store previous key state itself.
class Input {
public:
    static void hookWindow(GLFWwindow* w){
        s_window = w;
        glfwSetKeyCallback(w, onKey);
        glfwSetMouseButtonCallback(w, onMouseButton);
        glfwSetCursorPosCallback(w, onCursorPos);
        glfwSetScrollCallback(w, onScroll);
    }

    // Call once at the very start of each frame, before pollEvents
    static void beginFrame(){
        s_prevKeys = s_keys;
        s_prevMouse = s_mouse;
        s_mouseDelta = { 0.0f, 0.0f };
        s_scrollDelta = 0.0f;
    }

    // Keyboard
    static bool keyHeld(int key){ return s_keys[key]; }
    static bool keyDown(int key){ return s_keys[key] && !s_prevKeys[key]; }
    static bool keyUp(int key){ return !s_keys[key] &&  s_prevKeys[key]; }

    // Mouse buttons
    static bool mouseHeld(int btn){ return s_mouse[btn]; }
    static bool mouseDown(int btn){ return s_mouse[btn] && !s_prevMouse[btn]; }
    static bool mouseUp(int btn){ return !s_mouse[btn] &&  s_prevMouse[btn]; }

    // Mouse position and movement
    static glm::vec2 mousePos(){ return s_mousePos;   }
    static glm::vec2 mouseDelta(){ return s_mouseDelta; }
    static float scrollDelta(){ return s_scrollDelta; }

    static void captureCursor(bool capture) {
        glfwSetInputMode(s_window, GLFW_CURSOR,
            capture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }

private:
    inline static GLFWwindow* s_window = nullptr;
    inline static bool s_firstMouse = true;

    inline static std::unordered_map<int, bool> s_keys, s_prevKeys; 
    inline static std::unordered_map<int, bool> s_mouse, s_prevMouse;
    inline static glm::vec2 s_mousePos = { 0.0f, 0.0f };
    inline static glm::vec2 s_mouseDelta = { 0.0f, 0.0f };
    inline static float s_scrollDelta = 0.0f;

    static void onKey(GLFWwindow*, int key, int, int action, int) {
        if (key != GLFW_KEY_UNKNOWN)
            s_keys[key] = (action != GLFW_RELEASE);
    }
    static void onMouseButton(GLFWwindow*, int btn, int action, int) {
        s_mouse[btn] = (action != GLFW_RELEASE);
    }
    static void onCursorPos(GLFWwindow*, double x, double y) {
        glm::vec2 pos = { static_cast<float>(x), static_cast<float>(y) };
        if (s_firstMouse) { s_mousePos = pos; s_firstMouse = false; }
        s_mouseDelta = pos - s_mousePos;
        s_mousePos   = pos;
    }
    static void onScroll(GLFWwindow*, double, double y) {
        s_scrollDelta += static_cast<float>(y);
    }
};

} // namespace bs