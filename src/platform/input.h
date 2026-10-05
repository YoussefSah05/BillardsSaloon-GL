#pragma once

#include <glm/glm.hpp>

#include <array>

namespace BilliardsSaloon
{
    class Window;

    enum class InputDevice
    {
        KeyboardMouse,
        Gamepad
    };

    // Per-frame keyboard and mouse snapshot with press/release edge detection.
    // Call update() once per frame after polling events.
    class Input
    {
    public:
        void update(Window& window);

        // Keys are GLFW_KEY_* codes.
        [[nodiscard]] bool isDown(int key) const;
        [[nodiscard]] bool wasPressed(int key) const;

        // Buttons are GLFW_MOUSE_BUTTON_* codes (0-2 are tracked).
        [[nodiscard]] bool isMouseDown(int button) const;
        [[nodiscard]] bool wasMousePressed(int button) const;
        [[nodiscard]] bool wasMouseReleased(int button) const;

        // Any key, mouse button or gamepad button went down this frame.
        [[nodiscard]] bool anyPressed() const;

        // The first connected gamepad (GLFW's standard mapping, Xbox layout).
        // Buttons and axes are GLFW_GAMEPAD_* codes.
        [[nodiscard]] bool hasGamepad() const { return m_hasGamepad; }
        [[nodiscard]] bool gamepadDown(int button) const;
        [[nodiscard]] bool gamepadPressed(int button) const;

        // Sticks: -1..1 with a dead zone applied. Triggers: 0 (released) to 1.
        [[nodiscard]] float gamepadAxis(int axis) const;

        // The device the player touched last, for choosing prompt glyphs.
        [[nodiscard]] InputDevice lastDevice() const { return m_lastDevice; }

        // Cursor position in window coordinates (origin top-left).
        [[nodiscard]] glm::vec2 cursorPosition() const { return m_cursor; }

        // Cursor movement since the previous frame, in window coordinates.
        [[nodiscard]] glm::vec2 mouseDelta() const { return m_mouseDelta; }

        // Cursor position in normalized device coordinates (-1..1, y up).
        [[nodiscard]] glm::vec2 cursorNdc() const { return m_cursorNdc; }

        // Wheel steps this frame; positive scrolls away from the user.
        [[nodiscard]] float scrollDelta() const { return m_scrollDelta; }

        // Drop the next mouse delta, e.g. after capturing the cursor, which
        // makes GLFW report a jump in position.
        void discardNextMouseDelta() { m_discardMouseDelta = true; }

    private:
        static constexpr int KEY_COUNT = 349; // GLFW_KEY_LAST + 1
        static constexpr int MOUSE_BUTTON_COUNT = 3;
        static constexpr int GAMEPAD_BUTTON_COUNT = 15;   // GLFW_GAMEPAD_BUTTON_LAST + 1
        static constexpr int GAMEPAD_AXIS_COUNT = 6;      // GLFW_GAMEPAD_AXIS_LAST + 1

        void updateGamepad();

        [[nodiscard]] static bool keyInRange(int key);
        [[nodiscard]] static bool buttonInRange(int button);

        std::array<bool, KEY_COUNT> m_down {};
        std::array<bool, KEY_COUNT> m_previous {};
        std::array<bool, MOUSE_BUTTON_COUNT> m_mouseDown {};
        std::array<bool, MOUSE_BUTTON_COUNT> m_mousePrevious {};

        glm::vec2 m_cursor {0.0f};
        glm::vec2 m_cursorNdc {0.0f};
        glm::vec2 m_mouseDelta {0.0f};
        float m_scrollDelta {0.0f};
        std::array<bool, GAMEPAD_BUTTON_COUNT> m_padDown {};
        std::array<bool, GAMEPAD_BUTTON_COUNT> m_padPrevious {};
        std::array<float, GAMEPAD_AXIS_COUNT> m_padAxes {};
        bool m_hasGamepad {false};
        InputDevice m_lastDevice {InputDevice::KeyboardMouse};

        bool m_hasCursor {false};
        bool m_discardMouseDelta {true};
    };
}
