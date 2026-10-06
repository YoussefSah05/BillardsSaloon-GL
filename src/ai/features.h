#pragma once

#include "ai/ai_table.h"

#include <array>
#include <cstddef>
#include <vector>

namespace BilliardsSaloon::Ai
{
    // A fixed-size description of a table from one player's point of view,
    // shared by training (through the Python module) and the game (ONNX), so
    // both see exactly the same numbers.
    //
    // 16 ball slots (cue ball, then balls 1-15 by number), each:
    //   present, x / half-length, z / half-width, legal first target,
    //   in my group (8-ball), in the opponent's group, game ball, number / 15
    // then the context:
    //   discipline one-hot (3), phase one-hot (5), ball in hand one-hot (3),
    //   my turn, my fouls in a row / 3, opponent's / 3, push-out available,
    //   groups decided.
    inline constexpr std::size_t BALL_SLOTS = 16;
    inline constexpr std::size_t BALL_FEATURES = 8;
    inline constexpr std::size_t CONTEXT_FEATURES = 16;
    inline constexpr std::size_t FEATURE_COUNT = BALL_SLOTS * BALL_FEATURES + CONTEXT_FEATURES;

    [[nodiscard]] std::vector<float> encodeTable(const AiTable& table, int perspective);
}
