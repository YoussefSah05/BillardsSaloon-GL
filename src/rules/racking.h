#pragma once

#include "gameplay/game_variant.h"

#include <cstddef>
#include <random>
#include <vector>

namespace BilliardsSaloon::Rules
{
    // Rack slot indices follow buildRackPositions: row by row from the apex.
    // A 15-ball triangle has its middle at 4 and back corners at 10 and 14; a
    // 10-ball triangle has its middle at 4 and back corners at 6 and 9; a
    // 9-ball diamond has its middle at 4.
    //
    // Returns the ball number for each slot, shuffled as WPA racking requires:
    // - 8-ball: the 8 in the middle, a solid and a stripe in the back corners
    // - 9-ball: the 1 at the apex, the 9 in the middle
    // - 10-ball: the 1 at the apex, the 10 in the middle, the 2 and 3 on the back corners
    // Everything else is random.
    [[nodiscard]] std::vector<int> rackOrder(GameDiscipline discipline, std::vector<int> numbers, std::mt19937& random);
}
