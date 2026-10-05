#include "audio/shot_sounds.h"

#include "gameplay/sim_bridge.h"

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

    std::vector<SoundCue> planShotSounds(const Sim::ShotTrajectory& trajectory, double tableLength, double tableWidth)
    {
        std::vector<SoundCue> cues;
        const auto where = [&](const glm::dvec3& r) { return SimBridge::toGamePosition(r, tableLength, tableWidth); };

        for (std::size_t i = 0; i < trajectory.events.size(); ++i)
        {
            const Sim::ShotEvent& event = trajectory.events[i];
            if (event.type == Sim::EventType::Transition)
            {
                continue;
            }

            // States just before the event (the strike is judged just after).
            const double before = std::max(0.0, event.time - 1.0e-7);
            const std::vector<Sim::BallState> previous = trajectory.stateAt(before);
            const Sim::BallState& ball = previous[static_cast<std::size_t>(event.ball)];

            SoundCue cue;
            cue.time = event.time;
            cue.position = where(ball.r);

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
                    const Sim::BallState& other = previous[static_cast<std::size_t>(event.other)];
                    const glm::dvec2 normal = glm::normalize(planar(other.r - ball.r));
                    const double closing = glm::dot(planar(ball.v - other.v), normal);
                    cue.kind = SoundKind::BallBall;
                    cue.intensity = loudness(std::abs(closing), BALL_BALL_FULL);
                    cue.position = where(0.5 * (ball.r + other.r));
                    break;
                }
                case Sim::EventType::LinearCushion:
                case Sim::EventType::CircularCushion:
                    cue.kind = SoundKind::Cushion;
                    cue.intensity = loudness(glm::length(planar(ball.v)), CUSHION_FULL);
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

            // Merge with a cue of the same kind a moment earlier.
            if (!cues.empty() && (cues.back().kind == cue.kind) && (cue.time - cues.back().time < MERGE_SECONDS))
            {
                cues.back().intensity = std::max(cues.back().intensity, cue.intensity);
                continue;
            }
            cues.push_back(cue);
        }
        return cues;
    }
}
