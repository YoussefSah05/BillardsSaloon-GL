#include "platform/input.h"

#include <GLFW/glfw3.h>

#include <cstddef>

namespace BilliardsSaloon
{
    static_assert(GLFW_KEY_LAST + 1 == 349, "Update Input::KEY_COUNT for this GLFW version.");

    void Input::update(GLFWwindow* window)
    {
        m_previous = m_down;

        // GLFW key codes are sparse; codes below GLFW_KEY_SPACE are unused.
        for (int key = GLFW_KEY_SPACE; key < KEY_COUNT; ++key)
        {
            m_down[static_cast<std::size_t>(key)] = glfwGetKey(window, key) == GLFW_PRESS;
        }
    }

    bool Input::isDown(int key) const
    {
        return inRange(key) && m_down[static_cast<std::size_t>(key)];
    }

    bool Input::wasPressed(int key) const
    {
        return inRange(key) &&
               m_down[static_cast<std::size_t>(key)] &&
               !m_previous[static_cast<std::size_t>(key)];
    }

    bool Input::inRange(int key)
    {
        return (key >= 0) && (key < KEY_COUNT);
    }
}
