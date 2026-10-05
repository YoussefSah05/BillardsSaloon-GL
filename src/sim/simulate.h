#pragma once

#include "sim/motion.h"
#include "sim/resolve.h"
#include "sim/table.h"

#include <cstdint>
#include <vector>

namespace BilliardsSaloon::Sim
{
    enum class EventType : std::uint8_t
    {
        Strike,
        Transition,
        BallBall,
        LinearCushion,
        CircularCushion,
        Pocket
    };

    struct ShotEvent
    {
        EventType type {EventType::Strike};
        double time {0.0};
        int ball {-1};       // the ball involved (first ball for BallBall)
        int other {-1};      // second ball, or cushion / pocket index
        MotionState from {MotionState::Stationary};   // transitions only
        MotionState to {MotionState::Stationary};
    };

    // A whole shot, computed at the strike. events[i] is followed by
    // states[i], the state of every ball right after it; between events every
    // ball follows its closed-form motion, so stateAt() is exact at any time.
    struct ShotTrajectory
    {
        std::vector<ShotEvent> events;
        std::vector<std::vector<BallState>> states;
        BallParams params;
        bool complete {true};   // false if the event limit was reached

        [[nodiscard]] double duration() const;
        [[nodiscard]] std::vector<BallState> stateAt(double t) const;
        [[nodiscard]] const std::vector<BallState>& finalState() const { return states.back(); }
    };

    struct SimulationLimits
    {
        int maxEvents {20000};
    };

    // Strikes balls[cueBall] and simulates until everything is at rest or pocketed.
    [[nodiscard]] ShotTrajectory simulateShot(
        const Table& table,
        std::vector<BallState> balls,
        int cueBall,
        const CueStrike& strike,
        const BallParams& params,
        const CueSpecs& cue = {},
        const SimulationLimits& limits = {});
}
