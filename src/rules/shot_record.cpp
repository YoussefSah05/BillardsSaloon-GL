#include "rules/shot_record.h"

#include <algorithm>
#include <limits>

namespace BilliardsSaloon::Rules
{
    ShotRecord recordShot(const Sim::ShotTrajectory& trajectory, const std::vector<int>& ballNumbers)
    {
        ShotRecord record;
        double contactTime = std::numeric_limits<double>::infinity();
        std::vector<int> railBalls;

        const auto number = [&](int index) { return ballNumbers[static_cast<std::size_t>(index)]; };

        for (const Sim::ShotEvent& event : trajectory.events)
        {
            switch (event.type)
            {
                case Sim::EventType::BallBall:
                    if ((record.firstContact < 0) && ((number(event.ball) == CUE_BALL) || (number(event.other) == CUE_BALL)))
                    {
                        record.firstContact = (number(event.ball) == CUE_BALL) ? number(event.other) : number(event.ball);
                        contactTime = event.time;
                    }
                    break;

                case Sim::EventType::LinearCushion:
                case Sim::EventType::CircularCushion:
                {
                    const int ball = number(event.ball);
                    if (event.time >= contactTime)
                    {
                        record.railAfterContact = true;
                    }
                    if ((ball != CUE_BALL) && (std::find(railBalls.begin(), railBalls.end(), ball) == railBalls.end()))
                    {
                        railBalls.push_back(ball);
                    }
                    break;
                }

                case Sim::EventType::Pocket:
                    record.pots.push_back({number(event.ball), event.other});
                    break;

                case Sim::EventType::Strike:
                case Sim::EventType::Transition:
                    break;
            }
        }

        record.objectBallsToRail = static_cast<int>(railBalls.size());
        return record;
    }
}
