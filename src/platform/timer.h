#pragma once

#include <chrono>


namespace BilliardsSaloon
{
    class Timer
    {
    public:
        Timer();

        void reset();

        // Returns elapsed seconds since the previous tick.
        double tick();

        [[nodiscard]] double elapsedSeconds() const;

    private:
        using Clock = std::chrono::steady_clock;

        Clock::time_point m_startTime;
        Clock::time_point m_lastTickTime;
        double m_elapsedSeconds {0.0};
    };
}