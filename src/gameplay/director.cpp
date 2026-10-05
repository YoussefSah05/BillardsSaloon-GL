#include "gameplay/director.h"

#include "gameplay/sim_bridge.h"

#include <algorithm>

namespace BilliardsSaloon
{
    namespace
    {
        // How long before the drop the pocket camera comes in, and the
        // shortest time any shot is held on screen.
        constexpr double POCKET_LEAD_SECONDS = 1.1;
        constexpr double MIN_HOLD_SECONDS = 0.35;
        constexpr double WIDE_AFTER_SECONDS = 0.7;
        constexpr double WIDE_IF_LONGER_THAN = 2.2;
        constexpr double POCKET_HOLD_AFTER_DROP = 0.9;
    }

    std::vector<DirectorCut> planShotCoverage(const Sim::ShotTrajectory& trajectory, const DirectorTable& table)
    {
        std::vector<DirectorCut> cuts {DirectorCut{}};
        const double duration = trajectory.duration();

        // The first object ball to drop (the cue ball dropping is no highlight).
        const Sim::ShotEvent* drop = nullptr;
        for (const Sim::ShotEvent& event : trajectory.events)
        {
            if ((event.type == Sim::EventType::Pocket) && (event.ball != 0))
            {
                drop = &event;
                break;
            }
        }

        if ((drop != nullptr) && (drop->time > MIN_HOLD_SECONDS + 0.2) &&
            (drop->other >= 0) && (static_cast<std::size_t>(drop->other) < table.pockets.size()))
        {
            const glm::vec3 pocket = table.pockets[static_cast<std::size_t>(drop->other)];

            // Where the ball comes from: its position half a second earlier.
            const double before = std::max(0.0, drop->time - 0.5);
            const glm::dvec3 from = trajectory.stateAt(before)[static_cast<std::size_t>(drop->ball)].r;
            glm::vec3 approach = SimBridge::toGamePosition(from, table.length, table.width) - pocket;
            approach.y = 0.0f;
            approach = (glm::length(approach) > 1.0e-3f) ? glm::normalize(approach) : -glm::normalize(glm::vec3(pocket.x, 0.0f, pocket.z));

            DirectorCut pocketCut;
            pocketCut.time = std::max(MIN_HOLD_SECONDS, drop->time - POCKET_LEAD_SECONDS);
            pocketCut.shot = DirectorShot::Pocket;
            pocketCut.position = pocket - approach * 0.42f + glm::vec3(0.0f, 0.30f, 0.0f);
            pocketCut.target = pocket + approach * 0.30f;
            cuts.push_back(pocketCut);

            if (duration > drop->time + POCKET_HOLD_AFTER_DROP + MIN_HOLD_SECONDS)
            {
                DirectorCut wide;
                wide.time = drop->time + POCKET_HOLD_AFTER_DROP;
                wide.shot = DirectorShot::Wide;
                cuts.push_back(wide);
            }
            return cuts;
        }

        if (duration > WIDE_IF_LONGER_THAN)
        {
            DirectorCut wide;
            wide.time = WIDE_AFTER_SECONDS;
            wide.shot = DirectorShot::Wide;
            cuts.push_back(wide);
        }
        return cuts;
    }

    const DirectorCut& cutAt(const std::vector<DirectorCut>& cuts, double t)
    {
        const DirectorCut* current = &cuts.front();
        for (const DirectorCut& cut : cuts)
        {
            if (cut.time <= t)
            {
                current = &cut;
            }
        }
        return *current;
    }
}
