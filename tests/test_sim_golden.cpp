// Golden shots: the simulator against pooltool, the implementation it was
// ported from. Fixtures come from tools/golden/generate_golden_shots.py.
// Where pooltool's own reference is wrong (a known defect, see the generator),
// the fixture says how many events can be trusted and whether the final
// positions can.

#include "sim/simulate.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>
#include <string>
#include <vector>

using namespace BilliardsSaloon::Sim;

namespace
{
    using Json = nlohmann::json;

    // Final positions must agree to a millimetre; a shot is chaotic enough
    // that rounding differences can grow, but not past that.
    constexpr double POSITION_TOLERANCE = 1.0e-3;
    constexpr double TIME_TOLERANCE = 1.0e-3;

    Json loadFixtures()
    {
        std::ifstream stream(std::string(BS_TEST_DATA_DIR) + "/golden_shots.json");
        REQUIRE(stream.good());
        return Json::parse(stream);
    }

    const char* eventName(EventType type)
    {
        switch (type)
        {
            case EventType::Strike: return "strike";
            case EventType::BallBall: return "ball_ball";
            case EventType::LinearCushion: return "linear_cushion";
            case EventType::CircularCushion: return "circular_cushion";
            case EventType::Pocket: return "pocket";
            case EventType::Transition: return "transition";
        }
        return "?";
    }

    struct Collision
    {
        std::string type;
        double time {0.0};
        int ball {-1};
        int other {-1};
    };

    std::vector<Collision> collisions(const ShotTrajectory& trajectory)
    {
        std::vector<Collision> list;
        for (const ShotEvent& event : trajectory.events)
        {
            if (event.type == EventType::Transition)
            {
                continue;
            }
            Collision c {eventName(event.type), event.time, event.ball, -1};
            if (event.type == EventType::BallBall)
            {
                c.other = event.other;
            }
            list.push_back(c);
        }
        return list;
    }
}

TEST_CASE("golden shots match pooltool")
{
    const Json fixtures = loadFixtures();
    const Table table = buildPocketTable(PocketTableSpec{});
    const BallParams params;

    REQUIRE(fixtures.at("shots").size() >= 10);

    for (const Json& shot : fixtures.at("shots"))
    {
        const std::string name = shot.at("name");
        CAPTURE(name);

        std::vector<BallState> balls;
        for (const Json& xy : shot.at("balls"))
        {
            BallState state;
            state.r = glm::dvec3(xy[0].get<double>(), xy[1].get<double>(), params.R);
            balls.push_back(state);
        }

        const Json& s = shot.at("strike");
        const CueStrike strike {
            .speed = s.at("speed"),
            .phiDegrees = s.at("phi"),
            .thetaDegrees = s.value("theta", 0.0),
            .a = s.at("a"),
            .b = s.at("b")
        };

        const ShotTrajectory trajectory = simulateShot(table, balls, 0, strike, params);
        REQUIRE(trajectory.complete);

        // Same collisions and pockets, in the same order, at the same times.
        const std::vector<Collision> ours = collisions(trajectory);
        const Json& expected = shot.at("events");
        const bool finalValid = shot.at("final_valid");
        const std::size_t validEvents = shot.at("valid_events");
        if (finalValid)
        {
            CHECK(ours.size() == expected.size());
        }
        const std::size_t common = std::min(ours.size(), validEvents);
        CHECK(common == validEvents);
        for (std::size_t i = 0; i < common; ++i)
        {
            CAPTURE(i);
            const Json& e = expected[i];
            CHECK(ours[i].type == e.at("type").get<std::string>());
            CHECK(ours[i].time == doctest::Approx(e.at("time").get<double>()).epsilon(TIME_TOLERANCE));
            if (ours[i].type == "ball_ball")
            {
                const int a = e.at("ball");
                const int b = e.at("other");
                CHECK(((ours[i].ball == a && ours[i].other == b) || (ours[i].ball == b && ours[i].other == a)));
            }
            else
            {
                CHECK(ours[i].ball == e.at("ball").get<int>());
            }
        }

        if (!finalValid)
        {
            continue;
        }

        CHECK(trajectory.duration() == doctest::Approx(shot.at("duration").get<double>()).epsilon(TIME_TOLERANCE));

        const std::vector<BallState>& final = trajectory.finalState();
        const Json& expectedFinal = shot.at("final");
        REQUIRE(final.size() == expectedFinal.size());
        for (std::size_t i = 0; i < final.size(); ++i)
        {
            CAPTURE(i);
            const bool pocketed = expectedFinal[i].at("pocketed");
            CHECK((final[i].s == MotionState::Pocketed) == pocketed);
            if (!pocketed)
            {
                const Json& p = expectedFinal[i].at("position");
                const glm::dvec2 want(p[0].get<double>(), p[1].get<double>());
                const double error = glm::length(glm::dvec2(final[i].r) - want);
                CHECK(error < POSITION_TOLERANCE);
            }
        }
    }
}
