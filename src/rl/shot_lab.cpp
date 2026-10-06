#include "rl/shot_lab.h"

#include <algorithm>
#include <stdexcept>
#include <thread>

namespace BilliardsSaloon::Rl
{
    ShotOutcome outcomeOf(const Sim::ShotTrajectory& trajectory)
    {
        const std::vector<Sim::BallState>& final = trajectory.finalState();
        const std::size_t count = final.size();

        ShotOutcome outcome;
        outcome.pocketOf.assign(count, -1);
        outcome.cushionsBeforePot.assign(count, 0);
        outcome.events = static_cast<int>(trajectory.events.size());
        outcome.complete = trajectory.complete;

        for (const Sim::ShotEvent& event : trajectory.events)
        {
            const auto ball = static_cast<std::size_t>(event.ball);
            switch (event.type)
            {
                case Sim::EventType::BallBall:
                    if ((outcome.firstContact < 0) && ((event.ball == 0) || (event.other == 0)))
                    {
                        outcome.firstContact = (event.ball == 0) ? event.other : event.ball;
                    }
                    break;
                case Sim::EventType::LinearCushion:
                case Sim::EventType::CircularCushion:
                    if (outcome.pocketOf[ball] < 0)
                    {
                        ++outcome.cushionsBeforePot[ball];
                    }
                    break;
                case Sim::EventType::Pocket:
                    outcome.pocketOf[ball] = event.other;
                    outcome.potOrder.push_back(event.ball);
                    break;
                case Sim::EventType::Strike:
                case Sim::EventType::Transition:
                    break;
            }
        }

        outcome.finalPositions.reserve(count);
        for (const Sim::BallState& state : final)
        {
            outcome.finalPositions.emplace_back(state.r.x, state.r.y);
        }
        outcome.scratch = outcome.pocketOf.front() >= 0;
        return outcome;
    }

    ShotOutcome simulateOutcome(const Sim::Table& table, const std::vector<glm::dvec2>& positions,
                                const Sim::CueStrike& strike, const Sim::BallParams& params)
    {
        if (positions.empty())
        {
            throw std::invalid_argument("A layout needs at least the cue ball.");
        }
        std::vector<Sim::BallState> balls(positions.size());
        for (std::size_t i = 0; i < positions.size(); ++i)
        {
            balls[i].r = glm::dvec3(positions[i], params.R);
        }
        return outcomeOf(Sim::simulateShot(table, std::move(balls), 0, strike, params));
    }

    std::vector<ShotOutcome> simulateBatch(const Sim::Table& table, const std::vector<std::vector<glm::dvec2>>& layouts,
                                           const std::vector<Sim::CueStrike>& strikes, const Sim::BallParams& params,
                                           unsigned threads)
    {
        if (layouts.size() != strikes.size())
        {
            throw std::invalid_argument("simulateBatch needs one strike per layout.");
        }
        std::vector<ShotOutcome> outcomes(layouts.size());
        const unsigned available = std::max(1U, std::thread::hardware_concurrency());
        const std::size_t workers = std::min<std::size_t>((threads == 0) ? available : threads, std::max<std::size_t>(layouts.size(), 1));

        // Contiguous chunks; each shot is independent and the simulator is
        // pure, so threads share nothing but read-only inputs.
        const auto work = [&](std::size_t begin, std::size_t end)
        {
            for (std::size_t i = begin; i < end; ++i)
            {
                outcomes[i] = simulateOutcome(table, layouts[i], strikes[i], params);
            }
        };
        if (workers <= 1)
        {
            work(0, layouts.size());
            return outcomes;
        }
        std::vector<std::thread> pool;
        const std::size_t chunk = (layouts.size() + workers - 1) / workers;
        for (std::size_t w = 0; w < workers; ++w)
        {
            const std::size_t begin = w * chunk;
            const std::size_t end = std::min(layouts.size(), begin + chunk);
            if (begin < end)
            {
                pool.emplace_back(work, begin, end);
            }
        }
        for (std::thread& t : pool)
        {
            t.join();
        }
        return outcomes;
    }
}
