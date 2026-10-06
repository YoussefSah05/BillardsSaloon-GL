#pragma once

#include "sim/simulate.h"

#include <glm/glm.hpp>

#include <vector>

// Shot-level access to the simulator for reinforcement learning and
// baselines: any layout in, what happened out. No game session, no rules
// state, no rendering. Simulator frame and units (see rl/geometry.h).
namespace BilliardsSaloon::Rl
{
    // What one shot did. Ball indices follow the layout (0 = cue ball).
    struct ShotOutcome
    {
        std::vector<int> pocketOf;          // per ball: the pocket it dropped into, -1 if none
        std::vector<int> potOrder;          // balls in the order they dropped
        std::vector<int> cushionsBeforePot; // per ball: cushion contacts before it dropped (or in total)
        std::vector<glm::dvec2> finalPositions;
        bool scratch {false};               // the cue ball was pocketed
        int firstContact {-1};              // first ball the cue ball touched, -1 if none
        glm::dvec2 contactVelocity {0.0};   // that ball's velocity just after the contact (m/s)
        int events {0};
        bool complete {true};               // false if the simulator's event limit was reached

        [[nodiscard]] bool potted(int ball) const { return pocketOf[static_cast<std::size_t>(ball)] >= 0; }
    };

    // Reads the outcome from a simulated trajectory.
    [[nodiscard]] ShotOutcome outcomeOf(const Sim::ShotTrajectory& trajectory);

    // Places balls (at rest) at `positions` and plays `strike` on ball 0.
    [[nodiscard]] ShotOutcome simulateOutcome(const Sim::Table& table, const std::vector<glm::dvec2>& positions,
                                              const Sim::CueStrike& strike, const Sim::BallParams& params = {});

    // The same for many independent shots, spread over `threads` threads
    // (0: the hardware's count). Results are identical to calling
    // simulateOutcome one by one, in the same order.
    [[nodiscard]] std::vector<ShotOutcome> simulateBatch(const Sim::Table& table,
                                                         const std::vector<std::vector<glm::dvec2>>& layouts,
                                                         const std::vector<Sim::CueStrike>& strikes,
                                                         const Sim::BallParams& params = {}, unsigned threads = 0);
}
