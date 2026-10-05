#include "core/gamepad_math.h"

#include <algorithm>
#include <cmath>

namespace BilliardsSaloon
{
    float applyDeadZone(float value, float deadZone)
    {
        const float magnitude = std::abs(value);
        if ((magnitude <= deadZone) || (deadZone >= 1.0f))
        {
            return 0.0f;
        }

        const float scaled = std::min((magnitude - deadZone) / (1.0f - deadZone), 1.0f);
        return std::copysign(scaled, value);
    }

    int HoldRepeater::update(bool held, float deltaTimeSeconds)
    {
        if (!held)
        {
            m_wasHeld = false;
            m_timer = 0.0f;
            return 0;
        }

        if (!m_wasHeld)
        {
            m_wasHeld = true;
            m_timer = m_initialDelay;
            return 1;
        }

        m_timer -= deltaTimeSeconds;
        int steps = 0;
        while (m_timer <= 0.0f)
        {
            ++steps;
            m_timer += m_interval;
        }
        return steps;
    }
}
