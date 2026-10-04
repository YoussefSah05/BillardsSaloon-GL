#pragma once

#include <array>

struct GLFWwindow;

namespace BilliardsSaloon
{
    // Per-frame keyboard snapshot with press-edge detection.
    // Call update() once per frame after polling events.
    class Input
    {
    public:
        void update(GLFWwindow* window);

        // Key is currently held. Keys are GLFW_KEY_* codes.
        [[nodiscard]] bool isDown(int key) const;

        // Key went down this frame.
        [[nodiscard]] bool wasPressed(int key) const;

    private:
        static constexpr int KEY_COUNT = 349; // GLFW_KEY_LAST + 1

        [[nodiscard]] static bool inRange(int key);

        std::array<bool, KEY_COUNT> m_down {};
        std::array<bool, KEY_COUNT> m_previous {};
    };
}
