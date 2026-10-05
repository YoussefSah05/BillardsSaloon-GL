#include "gameplay/match_session.h"

#include "gameplay/turn_rules.h"
#include "physics/billiards_physics.h"
#include "scene/components.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace BilliardsSaloon
{
    namespace
    {
        // Distance of the cue ball start spot from the table centre, towards the player.
        constexpr float CUE_BALL_START_Z = 0.42f;

        TransformComponent restingTransform(const glm::vec3& position)
        {
            TransformComponent transform;
            transform.position = position;
            transform.syncPrevious();
            return transform;
        }

        glm::vec2 clampStrikeOffset(float right01, float forward01, float maxRadius01)
        {
            glm::vec2 offset(right01, forward01);
            const float length = glm::length(offset);

            if (length > maxRadius01)
            {
                offset = (offset / length) * maxRadius01;
            }

            return offset;
        }
    }

    glm::vec3 aimDirectionFromAngle(float angleRadians)
    {
        return glm::normalize(glm::vec3(std::sin(angleRadians), 0.0f, -std::cos(angleRadians)));
    }

    MatchSession::MatchSession(const GameVariantDefinition& variant, ShotInputTuning tuning)
        : m_variant(&variant)
        , m_tuning(tuning)
    {
        spawnTable();
        spawnBalls();
        resetRack();
    }

    glm::vec3 MatchSession::aimDirection() const
    {
        return aimDirectionFromAngle(m_shotState.aimAngleRadians);
    }

    glm::vec3 MatchSession::cueBallStartPosition() const
    {
        return glm::vec3(0.0f, m_variant->table.ballRadius, CUE_BALL_START_Z);
    }

    bool MatchSession::acceptsShotInput() const
    {
        return (m_matchState.flowPhase != MatchFlowPhase::FrameOver) &&
               (m_shotState.phase != ShotPhase::BallsInMotion);
    }

    bool MatchSession::ballsInMotion() const
    {
        // anyBallInMotion only reads, but the registry view API is non-const.
        return Physics::anyBallInMotion(const_cast<Registry&>(m_registry));
    }

    void MatchSession::spawnTable()
    {
        m_tableEntity = m_registry.createEntity();
        const TableSpecification& table = m_variant->table;

        m_registry.emplace<TableBoundsComponent>(m_tableEntity, TableBoundsComponent{
            .halfWidth = 0.5f * table.clothWidth,
            .halfDepth = 0.5f * table.clothDepth,
            .railRestitution = table.physics.cushionRestitution,
            .ballRestitution = table.physics.ballRestitution,
            .railContactFrictionCoefficient = table.physics.cushionFriction,
            .ballContactFrictionCoefficient = table.physics.ballFriction,
            .slidingFrictionCoefficient = table.physics.slidingFriction,
            .rollingFrictionCoefficient = table.physics.rollingFriction,
            .spinningFrictionCoefficient = table.physics.spinningFriction,
            .stopSpeedThreshold = table.physics.stopSpeed,
            .cornerPocketRadius = table.cornerPocketRadius,
            .sidePocketRadius = table.sidePocketRadius
        });
    }

    void MatchSession::spawnBalls()
    {
        auto spawn = [&](const BallSpawnDefinition& definition)
        {
            const Entity entity = m_registry.createEntity();
            m_registry.emplace<TransformComponent>(entity, restingTransform(glm::vec3(0.0f)));
            m_registry.emplace<BallComponent>(entity, BallComponent{
                .radius = m_variant->table.ballRadius,
                .massKg = m_variant->table.ballMassKg,
                .linearVelocity = glm::vec3(0.0f),
                .angularVelocity = glm::vec3(0.0f),
                .number = definition.number,
                .ruleTag = definition.ruleTag,
                .pocketed = false,
                .isCueBall = definition.isCueBall
            });
            return entity;
        };

        m_cueBallEntity = spawn(m_variant->cueBall);

        for (const BallSpawnDefinition& definition : m_variant->objectBalls)
        {
            m_objectBallEntities.push_back(spawn(definition));
        }
    }

    void MatchSession::resetRack()
    {
        m_matchState = MatchState{};
        m_matchState.discipline = m_variant->discipline;

        m_shotState = ShotState{};
        m_currentShotResult.clear();
        m_holdWasActive = false;
        m_chargingByStroke = false;

        resetCueBall();

        const std::vector<glm::vec3> rackPositions = buildRackPositions(*m_variant);

        for (std::size_t i = 0; i < m_objectBallEntities.size(); ++i)
        {
            m_registry.get<TransformComponent>(m_objectBallEntities[i]) = restingTransform(rackPositions[i]);

            BallComponent& ball = m_registry.get<BallComponent>(m_objectBallEntities[i]);
            ball.linearVelocity = glm::vec3(0.0f);
            ball.angularVelocity = glm::vec3(0.0f);
            ball.pocketed = false;
        }
    }

    void MatchSession::resetCueBall()
    {
        m_registry.get<TransformComponent>(m_cueBallEntity) = restingTransform(cueBallStartPosition());

        BallComponent& ball = m_registry.get<BallComponent>(m_cueBallEntity);
        ball.linearVelocity = glm::vec3(0.0f);
        ball.angularVelocity = glm::vec3(0.0f);
        ball.pocketed = false;

        m_shotState.strikeRight01 = 0.0f;
        m_shotState.strikeForward01 = 0.0f;
    }

    void MatchSession::applyShotControls(const ShotControls& controls, float deltaTimeSeconds)
    {
        const bool holding = controls.shootHeld || controls.strokeHeld;

        if (!acceptsShotInput())
        {
            m_holdWasActive = holding;
            return;
        }

        const float dt = std::max(deltaTimeSeconds, 0.0f);

        m_shotState.aimAngleRadians +=
            controls.aimAxis * m_tuning.aimRadiansPerSecond * dt + controls.aimDeltaRadians;
        m_shotState.strikeRight01 +=
            controls.strikeRightAxis * m_tuning.strikeOffsetPerSecond * dt + controls.strikeDelta.x;
        m_shotState.strikeForward01 +=
            controls.strikeForwardAxis * m_tuning.strikeOffsetPerSecond * dt + controls.strikeDelta.y;

        if (controls.centerStrike)
        {
            m_shotState.strikeRight01 = 0.0f;
            m_shotState.strikeForward01 = 0.0f;
        }

        const glm::vec2 clampedStrike = clampStrikeOffset(
            m_shotState.strikeRight01,
            m_shotState.strikeForward01,
            m_tuning.maxStrikeRadius01
        );
        m_shotState.strikeRight01 = clampedStrike.x;
        m_shotState.strikeForward01 = clampedStrike.y;

        if ((m_shotState.phase == ShotPhase::Aiming) && holding)
        {
            m_shotState.phase = ShotPhase::Charging;
            m_chargingByStroke = controls.strokeHeld && !controls.shootHeld;
        }

        if (m_shotState.phase == ShotPhase::Charging)
        {
            if (holding)
            {
                if (controls.shootHeld)
                {
                    m_shotState.charge01 += m_tuning.chargePerSecond * dt;
                }

                if (controls.strokeHeld)
                {
                    m_shotState.charge01 += controls.strokeDelta;
                }

                m_shotState.charge01 = std::clamp(m_shotState.charge01, 0.0f, 1.0f);
            }
            else if (m_holdWasActive)
            {
                if (m_chargingByStroke && (m_shotState.charge01 < m_tuning.strokeCancelBelow))
                {
                    m_shotState.phase = ShotPhase::Aiming;
                }
                else
                {
                    const bool fired = fireShot();
                    m_shotState.phase = fired ? ShotPhase::BallsInMotion : ShotPhase::Aiming;
                    m_matchState.shotInProgress = fired;
                }

                m_shotState.charge01 = 0.0f;
            }
        }

        m_holdWasActive = holding;
    }

    void MatchSession::cancelHeldShot()
    {
        m_holdWasActive = false;

        if (m_shotState.phase == ShotPhase::Charging)
        {
            m_shotState.phase = ShotPhase::Aiming;
            m_shotState.charge01 = 0.0f;
        }
    }

    void MatchSession::debugRespotCueBall()
    {
        resetCueBall();
        m_shotState.phase = ShotPhase::Aiming;
        m_shotState.charge01 = 0.0f;
        m_matchState.shotInProgress = false;
        m_currentShotResult.clear();
    }

    bool MatchSession::fireShot()
    {
        BallComponent& ball = m_registry.get<BallComponent>(m_cueBallEntity);
        if (ball.pocketed)
        {
            return false;
        }

        m_currentShotResult.clear();
        m_currentShotResult.shotActive = true;

        const glm::vec3 forward = aimDirection();
        const glm::vec3 up(0.0f, 1.0f, 0.0f);
        const glm::vec3 right = glm::normalize(glm::cross(up, forward));

        const float shotSpeed =
            m_tuning.minShotSpeed + (m_tuning.maxShotSpeed - m_tuning.minShotSpeed) * m_shotState.charge01;

        ball.linearVelocity = forward * shotSpeed;

        const float spinBase = shotSpeed / ball.radius;
        const glm::vec3 sideSpin = up * (m_shotState.strikeRight01 * 0.85f * spinBase);
        const glm::vec3 topBackSpin = right * (m_shotState.strikeForward01 * 1.00f * spinBase);

        ball.angularVelocity = sideSpin + topBackSpin;
        return true;
    }

    void MatchSession::step(double deltaTimeSeconds)
    {
        Physics::stepBilliardsWorld(m_registry, deltaTimeSeconds, m_currentShotResult);

        if ((m_shotState.phase != ShotPhase::BallsInMotion) || ballsInMotion())
        {
            return;
        }

        const int shooter = m_matchState.activePlayerIndex;
        const MatchFlowPhase phaseBefore = m_matchState.flowPhase;

        Rules::resolveShot(*m_variant, m_matchState, m_registry, m_currentShotResult);

        ShotOutcome outcome;
        outcome.shooter = shooter;
        outcome.nextPlayer = m_matchState.activePlayerIndex;
        outcome.foul = m_matchState.lastFoul;
        outcome.turnPassed = m_matchState.activePlayerIndex != shooter;
        outcome.groupsAssigned =
            (phaseBefore != MatchFlowPhase::GroupsAssigned) &&
            (m_matchState.flowPhase == MatchFlowPhase::GroupsAssigned);
        outcome.frameOver = m_matchState.flowPhase == MatchFlowPhase::FrameOver;
        outcome.winner = outcome.frameOver ? m_matchState.winnerPlayerIndex : -1;
        outcome.frameEnd = m_matchState.frameEnd;
        for (const PocketedBallRecord& record : m_currentShotResult.pocketedBalls)
        {
            if (!record.isCueBall)
            {
                outcome.pottedNumbers.push_back(record.number);
            }
        }
        m_lastOutcome = std::move(outcome);
        ++m_resolvedShotCount;

        if (m_matchState.flowPhase != MatchFlowPhase::FrameOver)
        {
            if (m_matchState.ballInHand)
            {
                resetCueBall();
            }

            m_shotState.phase = ShotPhase::Aiming;
        }

        m_currentShotResult.clear();
    }
}
