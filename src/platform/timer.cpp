#include "platform/timer.h"

// This file is part of BilliardsSaloon, licensed under the MIT License (MIT).
// We base our timing on std::chrono::steady_clock, which is guaranteed to be monotonic and not affected by system clock changes.

namespace BilliardsSaloon
{
    Timer::Timer()
    {
        reset();
    }

    void Timer::reset()
    {
        m_startTime = Clock::now();
        m_lastTickTime = m_startTime;
        m_elapsedSeconds = 0.0;
    }

    double Timer::tick()
    {
        const Clock::time_point now = Clock::now();
        const std::chrono::duration<double> delta = now - m_lastTickTime;
        
        m_lastTickTime = now;
        m_elapsedSeconds = delta.count();
        return m_elapsedSeconds;
    }

    double Timer::elapsedSeconds() const
    {
        return m_elapsedSeconds;
    }
}