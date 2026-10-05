#pragma once

#include "ecs/entity.h"
#include "ecs/registry.h"
#include "gameplay/game_variant.h"
#include "gameplay/shot_state.h"
#include "rules/match_score.h"
#include "rules/referee.h"
#include "scene/components.h"
#include "sim/simulate.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <random>
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

        // Cue speed at impact (the ball leaves at about 1.5x this) and how far
        // the tip can move from centre, as a fraction of R.
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

    struct MatchSettings
    {
        int raceTo {1};
        Rules::BreakOrder breakOrder {Rules::BreakOrder::Alternate};
        int firstBreaker {0};
        bool shuffleRack {true};          // WPA racking; false keeps the variant's order
        float shotClockSeconds {0.0f};    // 0 = no shot clock
        float extensionSeconds {30.0f};   // one per player per frame
        std::uint32_t seed {0x5EED};
    };

    // The shot as it would play if struck now, for the aim guides.
    struct ShotPreview
    {
        bool valid {false};
        bool contact {false};                 // the cue ball reaches an object ball
        glm::vec3 ghostBall {0.0f};           // cue ball centre at first contact
        int objectBall {-1};                  // number of the ball it reaches
        std::vector<glm::vec3> cuePath;       // cue ball centre, from now until it stops
        std::size_t cueContactIndex {0};      // cuePath index at first contact
        std::vector<glm::vec3> objectPath;    // the object ball after contact
        bool objectPotted {false};
        bool cuePotted {false};
    };

    // What happened on the last resolved shot, for the HUD and stats.
    struct ShotOutcome
    {
        Rules::Verdict verdict;
        int nextPlayer {0};
        bool matchOver {false};
    };

    // Headless owner of a match: the ECS world with table and balls, the shot
    // state machine, playback of simulated shots, the referee and the match score.
    // Presentation layers attach meshes and materials to its entities.
    class MatchSession
    {
    public:
        explicit MatchSession(
            const GameVariantDefinition& variant,
            ShotInputTuning tuning = {},
            MatchSettings settings = {});

        // The shot being played back, if any.
        [[nodiscard]] const Sim::ShotTrajectory* activeTrajectory() const
        {
            return m_trajectory ? &*m_trajectory : nullptr;
        }

        // The trajectory on screen (the live shot or a replay) and how far
        // into it playback is, in simulated seconds.
        [[nodiscard]] const Sim::ShotTrajectory* playbackTrajectory() const;
        [[nodiscard]] double playbackSeconds() const { return m_replay ? m_replay->seconds : m_playbackSeconds; }
        // Changes whenever a new shot or replay starts playing.
        [[nodiscard]] std::uint32_t playbackId() const { return m_playbackId; }

        // ---- Replays ---------------------------------------------------------
        // The last shot played again at speed (1 = real time), then the table
        // is put back exactly as it was. Allowed between shots and after the frame.
        [[nodiscard]] bool canReplay() const;
        bool startReplay(double speed);
        void stopReplay();
        [[nodiscard]] bool replaying() const { return m_replay.has_value(); }
        [[nodiscard]] double replaySpeed() const { return m_replay ? m_replay->speed : 1.0; }

        [[nodiscard]] Registry& registry() { return m_registry; }
        [[nodiscard]] const Registry& registry() const { return m_registry; }

        [[nodiscard]] const GameVariantDefinition& variant() const { return *m_variant; }
        [[nodiscard]] const MatchSettings& settings() const { return m_settings; }
        [[nodiscard]] const Rules::FrameState& frame() const { return m_frame; }
        [[nodiscard]] const Rules::MatchScore& score() const { return m_score; }
        [[nodiscard]] const ShotState& shotState() const { return m_shotState; }

        [[nodiscard]] Entity tableEntity() const { return m_tableEntity; }
        [[nodiscard]] Entity cueBallEntity() const { return m_cueBallEntity; }
        [[nodiscard]] const std::vector<Entity>& objectBallEntities() const { return m_objectBallEntities; }
        [[nodiscard]] std::optional<Entity> ballEntity(int number) const;

        // Object balls still on the table, by number, ascending.
        [[nodiscard]] std::vector<int> objectBallsOnTable() const;

        // Pocket centres in game coordinates, indexed like the simulator's pockets.
        [[nodiscard]] const std::vector<glm::vec3>& pocketPositions() const { return m_pocketPositions; }

        [[nodiscard]] glm::vec3 aimDirection() const;
        [[nodiscard]] glm::vec3 cueBallStartPosition() const;   // the head spot
        [[nodiscard]] glm::vec3 footSpot() const;
        [[nodiscard]] float headStringX() const;

        // True while the player may aim and shoot.
        [[nodiscard]] bool acceptsShotInput() const;
        [[nodiscard]] bool ballsInMotion() const;
        [[nodiscard]] bool frameOver() const { return m_frame.phase == Rules::Phase::FrameOver; }
        [[nodiscard]] bool matchOver() const { return m_score.over(); }

        // A new match: score reset, first frame racked.
        void startMatch();
        // Re-rack the current frame with the same breaker.
        void restartFrame();
        // After a frame is over: rack the next one (no-op once the match is over).
        void startNextFrame();
        [[nodiscard]] int frameNumber() const { return m_frameNumber; }

        // Puts the cue ball and the listed object balls at rest at the given
        // table positions (x, z); unlisted object balls leave the table.
        // For practice layouts, drills and tests; the frame state is unchanged.
        void setLayout(const glm::vec2& cueBall, const std::vector<std::pair<int, glm::vec2>>& objectBalls);

        // ---- Ball in hand -------------------------------------------------
        [[nodiscard]] bool placingCueBall() const { return m_shotState.phase == ShotPhase::PlacingCueBall; }
        // True when the shooter has ball in hand and has not shot yet.
        [[nodiscard]] bool canPlaceCueBall() const;
        void beginCueBallPlacement();
        // Moves the cue ball on the table plane (x, z), kept inside the allowed area.
        void moveCueBall(const glm::vec2& deltaXZ);
        [[nodiscard]] bool cueBallPlacementValid() const { return m_placementValid; }
        // Ends placement if the spot is legal; returns false otherwise.
        bool confirmCueBallPlacement();

        // ---- Referee choices ---------------------------------------------
        [[nodiscard]] Rules::Choice pendingChoice() const { return m_frame.choice; }
        [[nodiscard]] int chooser() const { return m_frame.chooser; }
        bool choose(Rules::Option option);

        // ---- Called shots ----------------------------------------------------
        // A called ball and pocket follow the aim automatically until the player
        // picks one by hand.
        [[nodiscard]] bool callRequired() const;
        [[nodiscard]] const std::optional<Rules::Call>& calledShot() const { return m_call; }
        void cycleCalledPocket(int direction);
        void cycleCalledBall(int direction);

        // ---- Shot clock --------------------------------------------------------
        // Runs while the shooter places, aims or strokes (not on the break,
        // never while paused or replaying). Running out is a foul.
        struct ShotClock
        {
            bool enabled {false};
            bool running {false};
            float remaining {0.0f};
            std::array<bool, 2> extensionAvailable {true, true};
        };
        [[nodiscard]] const ShotClock& shotClock() const { return m_clock; }
        bool useExtension();

        // ---- Push-out (9-ball, 10-ball) ------------------------------------------
        [[nodiscard]] bool pushOutAvailable() const;
        [[nodiscard]] bool pushOutDeclared() const { return m_pushOut; }
        void setPushOut(bool declared);

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

        // The predicted shot at the current aim, tip offset and power (the
        // power being charged, else the last shot's). Recomputed only when an
        // input changes; empty when no shot can be played.
        [[nodiscard]] const ShotPreview& shotPreview();

        // Increments each time a shot is resolved; compare to detect new outcomes.
        [[nodiscard]] std::uint32_t resolvedShotCount() const { return m_resolvedShotCount; }
        [[nodiscard]] const ShotOutcome& lastOutcome() const { return m_lastOutcome; }

    private:
        void spawnTable();
        void spawnBalls();
        void startFrame(int breaker);
        void rackBalls();
        void placeBall(Entity entity, const glm::vec3& position);
        void resetCueBall();
        void enterTurn();
        void resolveShot();
        void resetShotClock();
        void tickShotClock(double deltaTimeSeconds);

        [[nodiscard]] bool positionFree(const glm::vec3& position, Entity ignore) const;
        [[nodiscard]] glm::vec3 clampToPlacementArea(const glm::vec3& position) const;
        [[nodiscard]] bool placementLegal(const glm::vec3& position) const;
        [[nodiscard]] glm::vec3 nearestFreePlacement(const glm::vec3& preferred) const;
        void spotBall(int number);

        void updateAutoCall();
        [[nodiscard]] Sim::CueStrike currentStrike(float power01) const;
        [[nodiscard]] std::vector<Sim::BallState> simBallStates() const;

        bool fireShot();
        void playBack(double deltaTimeSeconds);
        void applySimStates(const std::vector<Sim::BallState>& states, double deltaTimeSeconds);

        const GameVariantDefinition* m_variant {nullptr};
        ShotInputTuning m_tuning;
        MatchSettings m_settings;
        std::mt19937 m_random;

        Registry m_registry;
        Entity m_tableEntity;
        Entity m_cueBallEntity;
        std::vector<Entity> m_objectBallEntities;

        Rules::FrameState m_frame {};
        Rules::MatchScore m_score {};
        int m_nextBreaker {0};
        int m_frameNumber {0};

        ShotState m_shotState {};
        bool m_placementValid {true};
        std::optional<Rules::Call> m_call;
        bool m_callBallByHand {false};
        bool m_callPocketByHand {false};
        bool m_pushOut {false};

        // What the referee needs about the shot in flight.
        std::vector<int> m_onTableAtStrike;
        std::optional<Rules::Call> m_callAtStrike;
        bool m_pushOutAtStrike {false};
        Rules::ShotRecord m_record {};

        Sim::Table m_simTable;
        std::vector<Entity> m_simBalls;   // simulator ball index -> entity (cue first)
        std::vector<int> m_simNumbers;    // simulator ball index -> ball number
        std::vector<glm::vec3> m_pocketPositions;
        std::optional<Sim::ShotTrajectory> m_trajectory;
        double m_playbackSeconds {0.0};

        struct BallSnapshot
        {
            TransformComponent transform;
            bool pocketed {false};
        };
        struct Replay
        {
            double seconds {0.0};
            double speed {1.0};
            std::vector<BallSnapshot> table;   // to restore afterwards
        };
        std::optional<Sim::ShotTrajectory> m_lastTrajectory;
        std::optional<Replay> m_replay;
        std::uint32_t m_playbackId {0};
        ShotClock m_clock;

        ShotPreview m_preview;
        glm::vec4 m_previewKey {-1.0f};      // aim, tip right, tip forward, power
        glm::vec3 m_previewCue {0.0f};
        float m_lastShotPower {0.5f};

        ShotOutcome m_lastOutcome {};
        std::uint32_t m_resolvedShotCount {0};
        bool m_holdWasActive {false};
        bool m_chargingByStroke {false};
    };
}
