#pragma once

#include "gameplay/sim_bridge.h"
#include "rules/referee.h"
#include "sim/resolve.h"

#include <glm/glm.hpp>

#include <cmath>
#include <optional>

namespace BilliardsSaloon
{
    // Shot input rates are per second so aiming and charging feel identical at
    // any frame rate. Values match the prototype's feel at 60 fps.
    struct ShotInputTuning
    {
        float aimRadiansPerSecond {0.9f};
        float strikeOffsetPerSecond {0.9f};
        float chargePerSecond {0.9f};
        float maxStrikeRadius01 {0.75f};

        // Cue speed at impact (the ball leaves at about 1.5x this) and how far
        // the tip can move from centre, as a fraction of R.
        float minCueSpeed {0.5f};
        float maxCueSpeed {7.0f};
        float tipOffsetPerStrikeUnit {0.7f};

        float elevationDegreesPerSecond {30.0f};
        float maxElevationDegrees {60.0f};

        // Releasing a mouse stroke below this power cancels instead of shooting.
        float strokeCancelBelow {0.03f};
    };

    // Everything a player decides for one shot, in the game's terms. Human
    // input and the AI both end up here.
    struct ShotInput
    {
        float aimRadians {0.0f};          // direction (sin a, 0, -cos a) in game x, z
        float power01 {0.5f};
        float strikeRight01 {0.0f};       // +1 right english
        float strikeForward01 {0.0f};     // +1 follow
        float elevationDegrees {0.0f};
        std::optional<Rules::Call> call;
        bool pushOut {false};
    };

    [[nodiscard]] inline glm::vec3 aimDirectionFromAngle(float angleRadians)
    {
        return glm::normalize(glm::vec3(std::sin(angleRadians), 0.0f, -std::cos(angleRadians)));
    }

    // The aim angle that points along a direction on the table.
    [[nodiscard]] inline float angleFromDirection(const glm::vec2& directionXZ)
    {
        return std::atan2(directionXZ.x, -directionXZ.y);
    }

    // The simulator's cue strike for a shot input.
    [[nodiscard]] inline Sim::CueStrike toCueStrike(const ShotInputTuning& tuning, const ShotInput& input)
    {
        return Sim::CueStrike {
            .speed = tuning.minCueSpeed + (tuning.maxCueSpeed - tuning.minCueSpeed) * input.power01,
            .phiDegrees = SimBridge::aimToPhiDegrees(aimDirectionFromAngle(input.aimRadians)),
            .thetaDegrees = input.elevationDegrees,
            // The simulator's a > 0 is left english; the game's strikeRight01 > 0 is right.
            .a = -input.strikeRight01 * tuning.tipOffsetPerStrikeUnit,
            .b = input.strikeForward01 * tuning.tipOffsetPerStrikeUnit
        };
    }
}
