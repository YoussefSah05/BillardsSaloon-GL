#pragma once

#include "ecs/entity.h"
#include "ecs/registry.h"
#include "gameplay/shot_state.h"
#include "platform/timer.h"
#include "platform/window.h"
#include "render/mesh.h"
#include "render/shader.h"

#include <cstdint>
#include <memory>

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

        Entity findCueBall() const;
        Entity findTable() const;
        bool anyBallInMotion() const;
        void resetCueBall();
        void fireCurrentShot();

        static constexpr double FIXED_TIME_STEP = 1.0 / 120.0;
        static constexpr double MAX_FRAME_TIME = 0.25;

        Window m_window;
        Timer m_timer;

        Registry m_registry;
        Entity m_cameraEntity;

        std::unique_ptr<Shader> m_basicShader;
        std::unique_ptr<Mesh> m_cubeMesh;
        std::unique_ptr<Mesh> m_planeMesh;
        std::unique_ptr<Mesh> m_sphereMesh;

        ShotState m_shotState {};
        bool m_spaceWasDownLastFrame {false};

        double m_accumulator {0.0};
        double m_simulationTime {0.0};
        std::uint64_t m_fixedFrameIndex {0};
    };
}