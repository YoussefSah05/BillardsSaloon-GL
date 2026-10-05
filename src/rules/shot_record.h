#pragma once

#include "rules/referee.h"
#include "sim/simulate.h"

#include <vector>

namespace BilliardsSaloon::Rules
{
    // Reads what the referee needs from a simulated shot. ballNumbers maps the
    // simulator's ball index to the ball's number (0 for the cue ball).
    [[nodiscard]] ShotRecord recordShot(const Sim::ShotTrajectory& trajectory, const std::vector<int>& ballNumbers);
}
