#pragma once

namespace BilliardsSaloon
{
    // Radial-style dead zone for one stick axis: values inside the dead zone
    // read as 0, the rest is rescaled so output still reaches ±1.
    [[nodiscard]] float applyDeadZone(float value, float deadZone);

    // Turns a held direction into repeated "steps", like a held arrow key:
    // one step when pressed, then after initialDelay one step every interval.
    class HoldRepeater
    {
    public:
        HoldRepeater(float initialDelaySeconds, float intervalSeconds)
            : m_initialDelay(initialDelaySeconds)
            , m_interval(intervalSeconds)
        {
        }

        // Returns how many steps to emit this frame.
        [[nodiscard]] int update(bool held, float deltaTimeSeconds);

    private:
        float m_initialDelay;
        float m_interval;
        float m_timer {0.0f};
        bool m_wasHeld {false};
    };
}
