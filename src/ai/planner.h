#pragma once

#include "ai/ai_profile.h"
#include "ai/ai_table.h"
#include "gameplay/shot_input.h"
#include "rules/referee.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <random>
#include <string>

namespace BilliardsSaloon::Ai
{
    // The AI's chosen shot and how good it expects it to be. score is roughly:
    // +100 winning the frame, +20..30 keeping the table with good position,
    // around 0 a fair safety, -40 a foul, -100 losing the frame.
    struct AiShot
    {
        ShotInput input;
        double score {0.0};
        bool safety {false};
        std::string description;   // e.g. "4 to FOOT LEFT, follow" (for logs and tests)
    };

    // Plans the best shot for the shooter in table.frame: candidate pots,
    // safeties and (when snookered) kicks, each tried on the simulator and
    // judged by the referee; the finalists are re-run with this player's
    // execution error and the best average wins. Deterministic per seed.
    [[nodiscard]] AiShot planShot(const AiTable& table, const AiProfile& profile, std::uint32_t seed);

    // The break: firm into the head ball of the rack.
    [[nodiscard]] AiShot planBreak(const AiTable& table, const AiProfile& profile);

    // With ball in hand: a legal spot that leaves the best shot.
    [[nodiscard]] glm::vec2 planCueBallPlacement(const AiTable& table, const AiProfile& profile, std::uint32_t seed);

    // The answer to a referee's choice put to this player (table.frame.chooser).
    [[nodiscard]] Rules::Option planChoice(const AiTable& table, const AiProfile& profile, std::uint32_t seed);

    // The shot as this player actually plays it: the plan plus their error.
    [[nodiscard]] ShotInput withExecutionError(ShotInput input, const AiProfile& profile, std::mt19937& random);

    // How good the position is for frame.shooter: 0 (nothing on) to 1 (an easy
    // shot, and more after it). Geometry only, no simulation.
    [[nodiscard]] double positionValue(const AiTable& table);
}
