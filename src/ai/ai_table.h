#pragma once

#include "gameplay/game_variant.h"
#include "gameplay/shot_input.h"
#include "rules/referee.h"
#include "sim/motion.h"
#include "sim/table.h"

#include <glm/glm.hpp>

#include <vector>

namespace BilliardsSaloon::Ai
{
    // A copy of everything the AI needs to plan a shot, so planning can run on
    // a worker thread while the game carries on. Positions are game x, z.
    struct AiTable
    {
        GameDiscipline discipline {GameDiscipline::EightBall};
        Rules::FrameState frame;
        std::vector<int> numbers;            // simulator index -> ball number; index 0 is the cue ball
        std::vector<glm::vec2> positions;
        std::vector<bool> pocketed;
        Sim::Table table;
        Sim::BallParams ball;
        ShotInputTuning tuning;
        float length {2.54f};                // cloth, game x
        float width {1.27f};                 // cloth, game z
        float radius {0.028575f};
        float headStringX {-0.635f};
        std::vector<glm::vec2> pockets;      // pocket centres, game x, z

        [[nodiscard]] std::vector<int> objectBallsOnTable() const;
        [[nodiscard]] int indexOf(int number) const;
        [[nodiscard]] glm::vec2 cueBall() const { return positions.front(); }
    };
}
