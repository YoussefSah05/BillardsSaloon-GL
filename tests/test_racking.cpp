#include "rules/racking.h"
#include "rules/referee.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <numeric>
#include <set>

using namespace BilliardsSaloon;

namespace
{
    std::vector<int> numbers(int first, int last)
    {
        std::vector<int> balls(static_cast<std::size_t>(last - first + 1));
        std::iota(balls.begin(), balls.end(), first);
        return balls;
    }

    bool sameBalls(std::vector<int> a, std::vector<int> b)
    {
        std::sort(a.begin(), a.end());
        std::sort(b.begin(), b.end());
        return a == b;
    }
}

TEST_CASE("racking follows WPA placement and is otherwise random")
{
    std::mt19937 random(1234);
    std::set<int> eightBallApexes;

    for (int i = 0; i < 200; ++i)
    {
        const std::vector<int> eight = Rules::rackOrder(GameDiscipline::EightBall, numbers(1, 15), random);
        REQUIRE(sameBalls(eight, numbers(1, 15)));
        CHECK(eight[4] == 8);
        CHECK(Rules::groupOf(eight[10]) != Rules::groupOf(eight[14]));
        CHECK(Rules::groupOf(eight[10]) != Rules::Group::None);
        eightBallApexes.insert(eight[0]);

        const std::vector<int> nine = Rules::rackOrder(GameDiscipline::NineBall, numbers(1, 9), random);
        REQUIRE(sameBalls(nine, numbers(1, 9)));
        CHECK(nine[0] == 1);
        CHECK(nine[4] == 9);

        const std::vector<int> ten = Rules::rackOrder(GameDiscipline::TenBall, numbers(1, 10), random);
        REQUIRE(sameBalls(ten, numbers(1, 10)));
        CHECK(ten[0] == 1);
        CHECK(ten[4] == 10);
        CHECK(((ten[6] == 2 && ten[9] == 3) || (ten[6] == 3 && ten[9] == 2)));
    }
    CHECK(eightBallApexes.size() > 5);
}

TEST_CASE("the same seed racks the same way")
{
    std::mt19937 a(99);
    std::mt19937 b(99);
    CHECK(Rules::rackOrder(GameDiscipline::NineBall, numbers(1, 9), a) ==
          Rules::rackOrder(GameDiscipline::NineBall, numbers(1, 9), b));
}
