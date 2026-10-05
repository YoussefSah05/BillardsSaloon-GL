#pragma once

#include "ecs/entity.h"
#include "ecs/registry.h"
#include "gameplay/game_variant.h"
#include "gameplay/match_state.h"
#include "gameplay/shot_result.h"
#include "gameplay/shot_state.h"
#include "sim/simulate.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <vector>

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
        float minShotSpeed {0.4f};        // legacy solver: ball speed, m/s
        float maxShotSpeed {3.8f};

        // Event-based simulator: cue speed at impact (the ball leaves at about
        // 1.5x this) and how far the tip can move from centre, as a fraction of R.
        float minCueSpeed {0.5f};
        float maxCueSpeed {7.0f};
        float tipOffsetPerStrikeUnit {0.7f};

        // Releasing a mouse stroke below this power cancels instead of shooting.
        float strokeCancelBelow {0.03f};
    };

    // One frame of player intent, already mapped from devices.
    // *Axis fields are rates (-1..1, scaled by frame time); *Delta fields are
    // direct changes this frame (from mouse motion).
    struct ShotControls
    {
        float aimAxis {0.0f};             // +1 rotates aim counter-clockwise (seen from above)
        float aimDeltaRadians {0.0f};
        float strikeRightAxis {0.0f};     // +1 moves the tip toward right english
        float strikeForwardAxis {0.0f};   // +1 moves the tip toward follow
        glm::vec2 strikeDelta {0.0f};     // (right, forward) in tip-offset units
        bool centerStrike {false};

        // Keyboard: hold to charge power over time, release to shoot.
        bool shootHeld {false};

        // Mouse: hold and drag back to set power, release to shoot.
        bool strokeHeld {false};
        float strokeDelta {0.0f};         // power change this frame; 1.0 = full range
    };

    [[nodiscard]] glm::vec3 aimDirectionFromAngle(float angleRadians);

    enum class PhysicsBackend
    {
        Legacy,       // fixed-step impulse solver (prototype)
        EventBased    // exact event simulation of the whole shot (bs_sim)
    };

    // What happened on the last resolved shot, for the referee and the HUD.
    struct ShotOutcome
    {
        int shooter {0};
        int nextPlayer {0};
        FoulReason foul {FoulReason::None};
        bool turnPassed {false};
        bool groupsAssigned {false};
        bool frameOver {false};
        int winner {-1};
        FrameEndReason frameEnd {FrameEndReason::None};
        std::vector<int> pottedNumbers;   // object balls only
    };

    // Headless owner of one rack of play: the ECS world with table and balls,
    // the shot state machine, physics stepping and rule resolution.
    // Presentation layers attach meshes and materials to its entities.
    class MatchSession
    {
    public:
        explicit MatchSession(
            const GameVariantDefinition& variant,
            ShotInputTuning tuning = {},
            PhysicsBackend backend = PhysicsBackend::EventBased);

        [[nodiscard]] PhysicsBackend backend() const { return m_backend; }

        // The shot being played back (event-based backend only).
        [[nodiscard]] const Sim::ShotTrajectory* activeTrajectory() const
        {
            return m_trajectory ? &*m_trajectory : nullptr;
        }

        [[nodiscard]] Registry& registry() { return m_registry; }
        [[nodiscard]] const Registry& registry() const { return m_registry; }

        [[nodiscard]] const GameVariantDefinition& variant() const { return *m_variant; }
        [[nodiscard]] const MatchState& matchState() const { return m_matchState; }
        [[nodiscard]] const ShotState& shotState() const { return m_shotState; }

        [[nodiscard]] Entity tableEntity() const { return m_tableEntity; }
        [[nodiscard]] Entity cueBallEntity() const { return m_cueBallEntity; }
        [[nodiscard]] const std::vector<Entity>& objectBallEntities() const { return m_objectBallEntities; }

        [[nodiscard]] glm::vec3 aimDirection() const;
        [[nodiscard]] glm::vec3 cueBallStartPosition() const;

        // True while the player may aim and shoot.
        [[nodiscard]] bool acceptsShotInput() const;
        [[nodiscard]] bool ballsInMotion() const;

        void resetRack();

        // Applies aiming, tip offset and the hold-to-charge / release-to-shoot
        // gesture for one frame of length deltaTimeSeconds.
        void applyShotControls(const ShotControls& controls, float deltaTimeSeconds);

        // Abandon a shot being charged, e.g. when the game is paused, so the
        // release after resuming does not fire it.
        void cancelHeldShot();

        // Development aid: put the cue ball back on its start spot mid-shot.
        void debugRespotCueBall();

        // Advances physics; resolves the shot by the rules once all balls stop.
        void step(double deltaTimeSeconds);

        // Increments each time a shot is resolved; compare to detect new outcomes.
        [[nodiscard]] std::uint32_t resolvedShotCount() const { return m_resolvedShotCount; }
        [[nodiscard]] const ShotOutcome& lastOutcome() const { return m_lastOutcome; }

    private:
        void spawnTable();
        void spawnBalls();
        void resetCueBall();
        bool fireShot();
        bool fireSimulatedShot();
        void playBack(double deltaTimeSeconds);
        void applySimStates(const std::vector<Sim::BallState>& states, double deltaTimeSeconds);

        const GameVariantDefinition* m_variant {nullptr};
        ShotInputTuning m_tuning;

        Registry m_registry;
        Entity m_tableEntity;
        Entity m_cueBallEntity;
        std::vector<Entity> m_objectBallEntities;

        MatchState m_matchState {};
        ShotState m_shotState {};
        ShotResult m_currentShotResult {};
        PhysicsBackend m_backend {PhysicsBackend::EventBased};
        Sim::Table m_simTable;
        std::vector<Entity> m_simBalls;   // simulator ball index -> entity (cue first)
        std::optional<Sim::ShotTrajectory> m_trajectory;
        double m_playbackSeconds {0.0};

        ShotOutcome m_lastOutcome {};
        std::uint32_t m_resolvedShotCount {0};
        bool m_holdWasActive {false};
        bool m_chargingByStroke {false};
    };
}
