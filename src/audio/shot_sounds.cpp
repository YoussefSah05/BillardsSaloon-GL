#include "audio/shot_sounds.h"

#include "gameplay/sim_bridge.h"
#include "sim/events.h"

#include <algorithm>
#include <cmath>

namespace BilliardsSaloon::Audio
{
    namespace
    {
        // Impact speeds (m/s) that sound at full volume.
        constexpr double BALL_BALL_FULL = 6.0;
        constexpr double CUSHION_FULL = 4.0;
        constexpr double POCKET_FULL = 3.0;
        constexpr double CUE_FULL = 9.0;
        constexpr float AUDIBLE = 0.03f;
        constexpr double MERGE_SECONDS = 0.006;

        // Perceived loudness grows more slowly than impact speed.
        float loudness(double speed, double full)
        {
            return static_cast<float>(std::sqrt(std::clamp(speed / full, 0.0, 1.0)));
        }

        glm::dvec2 planar(const glm::dvec3& v)
        {
            return glm::dvec2(v.x, v.y);
        }
    }

    std::vector<SoundCue> planShotSounds(const Sim::ShotTrajectory& trajectory, const Sim::Table& table)
    {
        std::vector<SoundCue> cues;
        cues.reserve(trajectory.events.size());
        const auto where = [&](const glm::dvec3& r) { return SimBridge::toGamePosition(r, table.length, table.width); };

        for (std::size_t i = 0; i < trajectory.events.size(); ++i)
        {
            const Sim::ShotEvent& event = trajectory.events[i];
            if (event.type == Sim::EventType::Transition)
            {
                continue;
            }

            // Evolve from the previous event, including preceding contacts at
            // this exact time. Sampling time-epsilon misses those impulses.
            const auto previous = [&](int index)
            {
                const std::size_t state = (i == 0) ? 0 : i - 1;
                return Sim::evolve(trajectory.states[state][static_cast<std::size_t>(index)], trajectory.params,
                                   std::max(0.0, event.time - trajectory.events[state].time));
            };
            const Sim::BallState ball = previous(event.ball);

            SoundCue cue;
            cue.time = event.time;
            cue.position = where(ball.r);
            cue.ball = event.ball;
            cue.other = event.other;

            switch (event.type)
            {
                case Sim::EventType::Strike:
                {
                    const Sim::BallState& struck = trajectory.states[i][static_cast<std::size_t>(event.ball)];
                    cue.kind = SoundKind::CueStrike;
                    cue.intensity = loudness(glm::length(planar(struck.v)), CUE_FULL);
                    cue.position = where(struck.r);
                    break;
                }
                case Sim::EventType::BallBall:
                {
                    const Sim::BallState other = previous(event.other);
                    const glm::dvec2 separation = planar(other.r - ball.r);
                    const double distance = glm::length(separation);
                    if (distance <= 1.0e-12)
                    {
                        continue;
                    }
                    const glm::dvec2 normal = separation / distance;
                    const double closing = glm::dot(planar(ball.v - other.v), normal);
                    cue.kind = SoundKind::BallBall;
                    cue.intensity = loudness(std::max(closing, 0.0), BALL_BALL_FULL);
                    cue.position = where(0.5 * (ball.r + other.r));
                    break;
                }
                case Sim::EventType::LinearCushion:
                    cue.kind = SoundKind::Cushion;
                    cue.intensity = loudness(std::abs(glm::dot(ball.v,
                        Sim::linearCushionNormal(ball, table.linear.at(static_cast<std::size_t>(event.other))))), CUSHION_FULL);
                    break;
                case Sim::EventType::CircularCushion:
                    cue.kind = SoundKind::PocketJaw;
                    cue.intensity = loudness(std::abs(glm::dot(ball.v,
                        Sim::circularCushionNormal(ball, table.circular.at(static_cast<std::size_t>(event.other))))), CUSHION_FULL);
                    break;
                case Sim::EventType::Pocket:
                    cue.kind = SoundKind::Pocket;
                    cue.intensity = std::max(0.35f, loudness(glm::length(planar(ball.v)), POCKET_FULL));
                    break;
                case Sim::EventType::Transition:
                    break;
            }

            if (cue.intensity < AUDIBLE)
            {
                continue;
            }

            bool merged = false;
            for (auto it = cues.rbegin(); it != cues.rend() && cue.time - it->time < MERGE_SECONDS; ++it)
            {
                const bool samePair = ((it->ball == cue.ball) && (it->other == cue.other)) ||
                    ((cue.kind == SoundKind::BallBall) && (it->ball == cue.other) && (it->other == cue.ball));
                if ((it->kind == cue.kind) && samePair && glm::length(it->position - cue.position) < 0.02f)
                {
                    it->intensity = std::max(it->intensity, cue.intensity);
                    merged = true;
                    break;
                }
            }
            if (!merged)
            {
                cues.push_back(cue);
            }
        }
        return cues;
    }
}
