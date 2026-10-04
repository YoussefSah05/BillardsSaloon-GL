#include "platform/input.h"

#include "platform/window.h"

#include <GLFW/glfw3.h>

#include <cstddef>

namespace BilliardsSaloon
{
    static_assert(GLFW_KEY_LAST + 1 == 349, "Update Input::KEY_COUNT for this GLFW version.");

    void Input::update(Window& window)
    {
        GLFWwindow* handle = window.nativeHandle();

        m_previous = m_down;
        m_mousePrevious = m_mouseDown;

        // GLFW key codes are sparse; codes below GLFW_KEY_SPACE are unused.
        for (int key = GLFW_KEY_SPACE; key < KEY_COUNT; ++key)
        {
            m_down[static_cast<std::size_t>(key)] = glfwGetKey(handle, key) == GLFW_PRESS;
        }

        for (int button = 0; button < MOUSE_BUTTON_COUNT; ++button)
        {
            m_mouseDown[static_cast<std::size_t>(button)] = glfwGetMouseButton(handle, button) == GLFW_PRESS;
        }

        double x = 0.0;
        double y = 0.0;
        glfwGetCursorPos(handle, &x, &y);
        const glm::vec2 cursor(static_cast<float>(x), static_cast<float>(y));

        m_mouseDelta = (m_hasCursor && !m_discardMouseDelta) ? (cursor - m_cursor) : glm::vec2(0.0f);
        m_cursor = cursor;
        m_hasCursor = true;
        m_discardMouseDelta = false;

        const glm::vec2 size = window.windowSize();
        m_cursorNdc = glm::vec2(2.0f * cursor.x / size.x - 1.0f, 1.0f - 2.0f * cursor.y / size.y);

        m_scrollDelta = window.consumeScrollDelta();
    }

    bool Input::isDown(int key) const
    {
        return keyInRange(key) && m_down[static_cast<std::size_t>(key)];
    }

    bool Input::wasPressed(int key) const
    {
        return keyInRange(key) &&
               m_down[static_cast<std::size_t>(key)] &&
               !m_previous[static_cast<std::size_t>(key)];
    }

    bool Input::isMouseDown(int button) const
    {
        return buttonInRange(button) && m_mouseDown[static_cast<std::size_t>(button)];
    }

    bool Input::wasMousePressed(int button) const
    {
        return buttonInRange(button) &&
               m_mouseDown[static_cast<std::size_t>(button)] &&
               !m_mousePrevious[static_cast<std::size_t>(button)];
    }

    bool Input::wasMouseReleased(int button) const
    {
        return buttonInRange(button) &&
               !m_mouseDown[static_cast<std::size_t>(button)] &&
               m_mousePrevious[static_cast<std::size_t>(button)];
    }

    bool Input::keyInRange(int key)
    {
        return (key >= 0) && (key < KEY_COUNT);
    }

    bool Input::buttonInRange(int button)
    {
        return (button >= 0) && (button < MOUSE_BUTTON_COUNT);
    }
}
