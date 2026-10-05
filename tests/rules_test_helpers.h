#pragma once

#include "rules/referee.h"

#include <initializer_list>
#include <numeric>
#include <vector>

namespace RulesTest
{
    using namespace BilliardsSaloon::Rules;

    // Builds a ShotRecord fluently: hit(1).pot(3, 2).rail()...
    struct Shot
    {
        ShotRecord record;

        Shot& hit(int ball) { record.firstContact = ball; return *this; }
        Shot& pot(int ball, int pocket = 0) { record.pots.push_back({ball, pocket}); return *this; }
        Shot& scratch(int pocket = 0) { return pot(CUE_BALL, pocket); }
        Shot& rail() { record.railAfterContact = true; return *this; }
        Shot& toRail(int balls) { record.objectBallsToRail = balls; record.railAfterContact = balls > 0; return *this; }
        Shot& push() { record.pushOut = true; return *this; }
        Shot& call(int ball, int pocket) { record.call = Call{ball, pocket}; return *this; }

        operator const ShotRecord&() const { return record; }
    };

    inline std::vector<int> balls(int first, int last)
    {
        std::vector<int> numbers(static_cast<std::size_t>(last - first + 1));
        std::iota(numbers.begin(), numbers.end(), first);
        return numbers;
    }

    inline std::vector<int> balls(std::initializer_list<int> numbers)
    {
        return numbers;
    }
}
