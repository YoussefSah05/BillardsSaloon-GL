#pragma once

#include "ecs/entity.h"
#include "ecs/registry.h"
#include "platform/timer.h"
#include "platform/window.h"

#include <cstdint>

namespace BilliardsSaloon
{
    class Application
    {
    public:
        Application();
        int run();

    private:
        void processPlatformInput();
        void updateFixed(double deltaTimeSeconds);
        void render(double alpha);

        static constexpr double FIXED_TIME_STEP = 1.0 / 120.0;
        static constexpr double MAX_FRAME_TIME = 0.25;

        Window m_window;
        Timer m_timer;

        Registry m_registry;
        Entity m_demoBall;

        double m_accumulator {0.0};
        double m_simulationTime {0.0};
        std::uint64_t m_fixedFrameIndex {0};
    };
}