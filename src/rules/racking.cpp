#include "rules/racking.h"

#include "rules/referee.h"

#include <algorithm>

namespace BilliardsSaloon::Rules
{
    namespace
    {
        constexpr std::size_t APEX = 0;
        constexpr std::size_t MIDDLE = 4;

        // Takes the first ball matching pred out of pool.
        template <typename Pred>
        int take(std::vector<int>& pool, Pred pred)
        {
            const auto it = std::find_if(pool.begin(), pool.end(), pred);
            if (it == pool.end())
            {
                return -1;
            }
            const int ball = *it;
            pool.erase(it);
            return ball;
        }
    }

    std::vector<int> rackOrder(GameDiscipline discipline, std::vector<int> numbers, std::mt19937& random)
    {
        const std::size_t count = numbers.size();
        std::vector<int> slots(count, -1);
        std::shuffle(numbers.begin(), numbers.end(), random);

        const auto place = [&](std::size_t slot, int ball)
        {
            if ((slot < count) && (ball >= 0))
            {
                slots[slot] = ball;
            }
        };
        const auto is = [](int number) { return [number](int ball) { return ball == number; }; };

        switch (discipline)
        {
            case GameDiscipline::EightBall:
            {
                place(MIDDLE, take(numbers, is(8)));
                // Which back corner gets the solid is random too.
                const bool solidLeft = std::uniform_int_distribution<int>(0, 1)(random) == 0;
                const int solid = take(numbers, [](int ball) { return groupOf(ball) == Group::Solids; });
                const int stripe = take(numbers, [](int ball) { return groupOf(ball) == Group::Stripes; });
                if (count == 15)
                {
                    place(solidLeft ? 10 : 14, solid);
                    place(solidLeft ? 14 : 10, stripe);
                }
                else
                {
                    numbers.push_back(solid);
                    numbers.push_back(stripe);
                }
                break;
            }

            case GameDiscipline::NineBall:
                place(APEX, take(numbers, is(1)));
                place(MIDDLE, take(numbers, is(9)));
                break;

            case GameDiscipline::TenBall:
            {
                place(APEX, take(numbers, is(1)));
                place(MIDDLE, take(numbers, is(10)));
                const bool twoLeft = std::uniform_int_distribution<int>(0, 1)(random) == 0;
                if (count == 10)
                {
                    place(twoLeft ? 6 : 9, take(numbers, is(2)));
                    place(twoLeft ? 9 : 6, take(numbers, is(3)));
                }
                break;
            }
        }

        // Fill the remaining slots with whatever is left, already shuffled.
        std::erase(numbers, -1);
        std::size_t next = 0;
        for (int& slot : slots)
        {
            if ((slot < 0) && (next < numbers.size()))
            {
                slot = numbers[next++];
            }
        }
        return slots;
    }
}
