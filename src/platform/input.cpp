#include "platform/input.h"

#include "core/gamepad_math.h"
#include "platform/window.h"

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstddef>

namespace BilliardsSaloon
{
    static_assert(GLFW_KEY_LAST + 1 == 349, "Update Input::KEY_COUNT for this GLFW version.");
    static_assert(GLFW_GAMEPAD_BUTTON_LAST + 1 == 15, "Update Input::GAMEPAD_BUTTON_COUNT.");
    static_assert(GLFW_GAMEPAD_AXIS_LAST + 1 == 6, "Update Input::GAMEPAD_AXIS_COUNT.");

    namespace
    {
        constexpr float STICK_DEAD_ZONE = 0.2f;
        constexpr float DEVICE_SWITCH_STICK = 0.5f;
        constexpr float DEVICE_SWITCH_MOUSE_POINTS = 3.0f;
    }

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

        updateGamepad();

        // Prompts follow whichever device was used last.
        bool keyboardOrMouseUsed = (glm::length(m_mouseDelta) > DEVICE_SWITCH_MOUSE_POINTS) || (m_scrollDelta != 0.0f);
        for (std::size_t i = 0; (i < m_down.size()) && !keyboardOrMouseUsed; ++i)
        {
            keyboardOrMouseUsed = m_down[i] && !m_previous[i];
        }
        for (std::size_t i = 0; (i < m_mouseDown.size()) && !keyboardOrMouseUsed; ++i)
        {
            keyboardOrMouseUsed = m_mouseDown[i] && !m_mousePrevious[i];
        }

        bool gamepadUsed = false;
        for (std::size_t i = 0; (i < m_padDown.size()) && !gamepadUsed; ++i)
        {
            gamepadUsed = m_padDown[i] && !m_padPrevious[i];
        }
        for (int axis = GLFW_GAMEPAD_AXIS_LEFT_X; (axis <= GLFW_GAMEPAD_AXIS_RIGHT_Y) && !gamepadUsed; ++axis)
        {
            gamepadUsed = std::abs(m_padAxes[static_cast<std::size_t>(axis)]) > DEVICE_SWITCH_STICK;
        }

        if (gamepadUsed)
        {
            m_lastDevice = InputDevice::Gamepad;
        }
        else if (keyboardOrMouseUsed)
        {
            m_lastDevice = InputDevice::KeyboardMouse;
        }
    }

    void Input::updateGamepad()
    {
        m_padPrevious = m_padDown;
        m_hasGamepad = false;

        for (int joystick = GLFW_JOYSTICK_1; joystick <= GLFW_JOYSTICK_LAST; ++joystick)
        {
            GLFWgamepadstate state;
            if ((glfwJoystickIsGamepad(joystick) == GLFW_TRUE) && (glfwGetGamepadState(joystick, &state) == GLFW_TRUE))
            {
                m_hasGamepad = true;
                for (std::size_t i = 0; i < m_padDown.size(); ++i)
                {
                    m_padDown[i] = state.buttons[i] == GLFW_PRESS;
                }
                for (std::size_t i = 0; i < m_padAxes.size(); ++i)
                {
                    m_padAxes[i] = state.axes[i];
                }
                return;
            }
        }

        m_padDown.fill(false);
        m_padAxes.fill(0.0f);
        // Released triggers rest at -1 in GLFW's mapping.
        m_padAxes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] = -1.0f;
        m_padAxes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] = -1.0f;
    }

    bool Input::gamepadDown(int button) const
    {
        return (button >= 0) && (button < GAMEPAD_BUTTON_COUNT) && m_padDown[static_cast<std::size_t>(button)];
    }

    bool Input::gamepadPressed(int button) const
    {
        return (button >= 0) && (button < GAMEPAD_BUTTON_COUNT) &&
               m_padDown[static_cast<std::size_t>(button)] && !m_padPrevious[static_cast<std::size_t>(button)];
    }

    float Input::gamepadAxis(int axis) const
    {
        if ((axis < 0) || (axis >= GAMEPAD_AXIS_COUNT))
        {
            return 0.0f;
        }

        const float value = m_padAxes[static_cast<std::size_t>(axis)];
        if ((axis == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER) || (axis == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER))
        {
            return (value + 1.0f) * 0.5f;
        }
        return applyDeadZone(value, STICK_DEAD_ZONE);
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

    bool Input::anyPressed() const
    {
        for (std::size_t i = 0; i < m_down.size(); ++i)
        {
            if (m_down[i] && !m_previous[i])
            {
                return true;
            }
        }
        for (std::size_t i = 0; i < m_mouseDown.size(); ++i)
        {
            if (m_mouseDown[i] && !m_mousePrevious[i])
            {
                return true;
            }
        }
        for (std::size_t i = 0; i < m_padDown.size(); ++i)
        {
            if (m_padDown[i] && !m_padPrevious[i])
            {
                return true;
            }
        }
        return false;
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
