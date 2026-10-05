#include "gameplay/match_session.h"

#include "gameplay/sim_bridge.h"
#include "rules/racking.h"
#include "rules/shot_record.h"
#include "scene/components.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace BilliardsSaloon
{
    namespace
    {
        constexpr float PI = 3.14159265358979323846f;

        // Extra clearance between a placed or spotted ball and its neighbours.
        constexpr float PLACEMENT_GAP = 0.0005f;

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

        // Distance along the ray (origin, direction) at which a ball of radius
        // R first touches a ball of radius R at center, or a negative number.
        float contactDistance(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& center, float radius)
        {
            const glm::vec3 offset = center - origin;
            const float along = glm::dot(offset, direction);
            const float reach = 2.0f * radius;
            const float missSquared = glm::dot(offset, offset) - along * along;
            if ((along <= 0.0f) || (missSquared > reach * reach))
            {
                return -1.0f;
            }
            return along - std::sqrt(reach * reach - missSquared);
        }
    }

    glm::vec3 aimDirectionFromAngle(float angleRadians)
    {
        return glm::normalize(glm::vec3(std::sin(angleRadians), 0.0f, -std::cos(angleRadians)));
    }

    MatchSession::MatchSession(const GameVariantDefinition& variant, ShotInputTuning tuning, MatchSettings settings)
        : m_variant(&variant)
        , m_tuning(tuning)
        , m_settings(settings)
        , m_random(settings.seed)
        , m_simTable(Sim::buildPocketTable(variant.table.pocketGeometry))
    {
        for (const Sim::Pocket& pocket : m_simTable.pockets)
        {
            glm::vec3 position = SimBridge::toGamePosition(pocket.center, variant.table.clothWidth, variant.table.clothDepth);
            position.y = 0.0f;
            m_pocketPositions.push_back(position);
        }

        spawnTable();
        spawnBalls();
        startMatch();
    }

    glm::vec3 MatchSession::aimDirection() const
    {
        return aimDirectionFromAngle(m_shotState.aimAngleRadians);
    }

    glm::vec3 MatchSession::cueBallStartPosition() const
    {
        // The head spot: a quarter of the table's length from the centre.
        return glm::vec3(headStringX(), m_variant->table.ballRadius, 0.0f);
    }

    glm::vec3 MatchSession::footSpot() const
    {
        return glm::vec3(0.25f * m_variant->table.clothWidth, m_variant->table.ballRadius, 0.0f);
    }

    float MatchSession::headStringX() const
    {
        return -0.25f * m_variant->table.clothWidth;
    }

    std::optional<Entity> MatchSession::ballEntity(int number) const
    {
        for (std::size_t i = 0; i < m_simBalls.size(); ++i)
        {
            if (m_simNumbers[i] == number)
            {
                return m_simBalls[i];
            }
        }
        return std::nullopt;
    }

    std::vector<int> MatchSession::objectBallsOnTable() const
    {
        std::vector<int> numbers;
        for (const Entity entity : m_objectBallEntities)
        {
            const BallComponent& ball = m_registry.get<BallComponent>(entity);
            if (!ball.pocketed)
            {
                numbers.push_back(ball.number);
            }
        }
        std::sort(numbers.begin(), numbers.end());
        return numbers;
    }

    bool MatchSession::acceptsShotInput() const
    {
        return !frameOver() && !m_replay && (m_frame.choice == Rules::Choice::None) &&
               ((m_shotState.phase == ShotPhase::Aiming) || (m_shotState.phase == ShotPhase::Charging));
    }

    bool MatchSession::ballsInMotion() const
    {
        return m_trajectory.has_value();
    }

    void MatchSession::spawnTable()
    {
        // The table's geometry lives in m_simTable; the entity carries its look.
        m_tableEntity = m_registry.createEntity();
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

        m_simBalls.push_back(m_cueBallEntity);
        m_simBalls.insert(m_simBalls.end(), m_objectBallEntities.begin(), m_objectBallEntities.end());
        m_simNumbers.push_back(Rules::CUE_BALL);
        for (const BallSpawnDefinition& definition : m_variant->objectBalls)
        {
            m_simNumbers.push_back(definition.number);
        }
    }

    // ---- Frames and the match ------------------------------------------------

    void MatchSession::startMatch()
    {
        m_score = Rules::MatchScore{};
        m_score.raceTo = std::max(1, m_settings.raceTo);
        m_score.order = m_settings.breakOrder;
        m_frameNumber = 0;
        startFrame(std::clamp(m_settings.firstBreaker, 0, 1));
    }

    void MatchSession::restartFrame()
    {
        --m_frameNumber;
        startFrame(m_frame.breaker);
    }

    void MatchSession::startNextFrame()
    {
        if (frameOver() && !matchOver())
        {
            startFrame(m_nextBreaker);
        }
    }

    void MatchSession::startFrame(int breaker)
    {
        ++m_frameNumber;
        m_frame = Rules::startFrame(m_variant->discipline, breaker);
        m_clock.extensionAvailable = {true, true};
        rackBalls();
    }

    void MatchSession::rackBalls()
    {
        m_replay.reset();
        m_lastTrajectory.reset();
        m_trajectory.reset();
        m_holdWasActive = false;
        m_chargingByStroke = false;

        m_shotState = ShotState{};
        m_shotState.aimAngleRadians = 0.5f * PI;   // towards the rack, along +x

        // Slot i of the rack gets ball order[i].
        std::vector<int> order;
        for (const BallSpawnDefinition& definition : m_variant->objectBalls)
        {
            order.push_back(definition.number);
        }
        if (m_settings.shuffleRack)
        {
            order = Rules::rackOrder(m_variant->discipline, order, m_random);
        }

        const std::vector<glm::vec3> slots = buildRackPositions(*m_variant);
        for (std::size_t slot = 0; slot < slots.size(); ++slot)
        {
            if (const std::optional<Entity> entity = ballEntity(order[slot]))
            {
                placeBall(*entity, slots[slot]);
            }
        }

        resetCueBall();
        enterTurn();
    }

    void MatchSession::setLayout(const glm::vec2& cueBall, const std::vector<std::pair<int, glm::vec2>>& objectBalls)
    {
        const float r = m_variant->table.ballRadius;
        m_trajectory.reset();
        for (const Entity entity : m_objectBallEntities)
        {
            placeBall(entity, glm::vec3(0.0f, r, 0.0f));
            m_registry.get<BallComponent>(entity).pocketed = true;
        }
        for (const auto& [number, position] : objectBalls)
        {
            if (const std::optional<Entity> entity = ballEntity(number))
            {
                placeBall(*entity, glm::vec3(position.x, r, position.y));
            }
        }
        placeBall(m_cueBallEntity, glm::vec3(cueBall.x, r, cueBall.y));
        m_placementValid = placementLegal(glm::vec3(cueBall.x, r, cueBall.y));
        updateAutoCall();
    }

    void MatchSession::placeBall(Entity entity, const glm::vec3& position)
    {
        m_registry.get<TransformComponent>(entity) = restingTransform(position);
        BallComponent& ball = m_registry.get<BallComponent>(entity);
        ball.linearVelocity = glm::vec3(0.0f);
        ball.angularVelocity = glm::vec3(0.0f);
        ball.pocketed = false;
    }

    void MatchSession::resetCueBall()
    {
        placeBall(m_cueBallEntity, cueBallStartPosition());
        m_shotState.strikeRight01 = 0.0f;
        m_shotState.strikeForward01 = 0.0f;
    }

    // Sets up the next shot: the cue ball in hand after a foul, a fresh call
    // and no push-out declared.
    void MatchSession::enterTurn()
    {
        m_call.reset();
        m_callBallByHand = false;
        m_callPocketByHand = false;
        m_pushOut = false;
        m_shotState.phase = ShotPhase::Aiming;
        m_shotState.charge01 = 0.0f;
        resetShotClock();

        if (frameOver() || (m_frame.choice != Rules::Choice::None))
        {
            return;
        }

        if (m_frame.ballInHand != Rules::BallInHand::None)
        {
            const TransformComponent& transform = m_registry.get<TransformComponent>(m_cueBallEntity);
            const bool pocketed = m_registry.get<BallComponent>(m_cueBallEntity).pocketed;
            const glm::vec3 preferred = pocketed ? cueBallStartPosition() : transform.position;
            placeBall(m_cueBallEntity, nearestFreePlacement(preferred));
            m_placementValid = true;

            // The break starts aimed from the head spot; placing is optional.
            // After a foul the player places first.
            if (m_frame.phase != Rules::Phase::Break)
            {
                m_shotState.phase = ShotPhase::PlacingCueBall;
            }
        }

        updateAutoCall();
    }

    // ---- Ball in hand ----------------------------------------------------------

    bool MatchSession::positionFree(const glm::vec3& position, Entity ignore) const
    {
        const float minimum = 2.0f * m_variant->table.ballRadius + PLACEMENT_GAP;
        for (const Entity entity : m_simBalls)
        {
            if (entity == ignore)
            {
                continue;
            }
            const BallComponent& ball = m_registry.get<BallComponent>(entity);
            if (ball.pocketed)
            {
                continue;
            }
            const glm::vec3 other = m_registry.get<TransformComponent>(entity).position;
            const glm::vec2 offset(position.x - other.x, position.z - other.z);
            if (glm::dot(offset, offset) < minimum * minimum)
            {
                return false;
            }
        }
        return true;
    }

    glm::vec3 MatchSession::clampToPlacementArea(const glm::vec3& position) const
    {
        const TableSpecification& table = m_variant->table;
        const float r = table.ballRadius;
        const float maxX = (m_frame.ballInHand == Rules::BallInHand::BehindHeadString)
            ? headStringX()
            : 0.5f * table.clothWidth - r;

        return glm::vec3(
            std::clamp(position.x, -0.5f * table.clothWidth + r, maxX),
            r,
            std::clamp(position.z, -0.5f * table.clothDepth + r, 0.5f * table.clothDepth - r));
    }

    bool MatchSession::placementLegal(const glm::vec3& position) const
    {
        return positionFree(position, m_cueBallEntity);
    }

    glm::vec3 MatchSession::nearestFreePlacement(const glm::vec3& preferred) const
    {
        const glm::vec3 start = clampToPlacementArea(preferred);
        if (placementLegal(start))
        {
            return start;
        }

        // Search rings of growing radius around the preferred spot.
        const float step = m_variant->table.ballRadius;
        for (int ring = 1; ring < 80; ++ring)
        {
            const int samples = 8 * ring;
            for (int i = 0; i < samples; ++i)
            {
                const float angle = 2.0f * PI * static_cast<float>(i) / static_cast<float>(samples);
                const glm::vec3 candidate = clampToPlacementArea(
                    start + static_cast<float>(ring) * step * glm::vec3(std::cos(angle), 0.0f, std::sin(angle)));
                if (placementLegal(candidate))
                {
                    return candidate;
                }
            }
        }
        return start;
    }

    bool MatchSession::canPlaceCueBall() const
    {
        return !frameOver() && !m_replay && (m_frame.choice == Rules::Choice::None) &&
               (m_frame.ballInHand != Rules::BallInHand::None) &&
               ((m_shotState.phase == ShotPhase::Aiming) || (m_shotState.phase == ShotPhase::PlacingCueBall));
    }

    void MatchSession::beginCueBallPlacement()
    {
        if (canPlaceCueBall())
        {
            m_shotState.phase = ShotPhase::PlacingCueBall;
            m_shotState.charge01 = 0.0f;
        }
    }

    void MatchSession::moveCueBall(const glm::vec2& deltaXZ)
    {
        if (!placingCueBall())
        {
            return;
        }

        TransformComponent& transform = m_registry.get<TransformComponent>(m_cueBallEntity);
        transform.syncPrevious();
        transform.position = clampToPlacementArea(transform.position + glm::vec3(deltaXZ.x, 0.0f, deltaXZ.y));
        m_placementValid = placementLegal(transform.position);
        updateAutoCall();
    }

    bool MatchSession::confirmCueBallPlacement()
    {
        if (!placingCueBall() || !m_placementValid)
        {
            return false;
        }
        m_registry.get<TransformComponent>(m_cueBallEntity).syncPrevious();
        m_shotState.phase = ShotPhase::Aiming;
        m_holdWasActive = true;   // the click that confirmed must not start a stroke
        return true;
    }

    void MatchSession::spotBall(int number)
    {
        const std::optional<Entity> entity = ballEntity(number);
        if (!entity)
        {
            return;
        }

        // WPA: on the foot spot, or as close as possible behind it on the long
        // string (towards the foot rail), else in front of it.
        const float r = m_variant->table.ballRadius;
        const float footRail = 0.5f * m_variant->table.clothWidth - r;
        const glm::vec3 spot = footSpot();
        const float step = 0.002f;

        m_registry.get<BallComponent>(*entity).pocketed = true;   // ignore itself while searching
        for (float x = spot.x; x <= footRail; x += step)
        {
            const glm::vec3 candidate(x, r, 0.0f);
            if (positionFree(candidate, *entity))
            {
                placeBall(*entity, candidate);
                return;
            }
        }
        for (float x = spot.x; x >= -footRail; x -= step)
        {
            const glm::vec3 candidate(x, r, 0.0f);
            if (positionFree(candidate, *entity))
            {
                placeBall(*entity, candidate);
                return;
            }
        }
        placeBall(*entity, spot);
    }

    // ---- Choices, calls and push-outs -------------------------------------------

    bool MatchSession::choose(Rules::Option option)
    {
        const Rules::ChoiceResult result = Rules::applyChoice(m_frame, option);
        if (!result.accepted)
        {
            return false;
        }
        if (result.rerack)
        {
            rackBalls();
        }
        else
        {
            enterTurn();
        }
        return true;
    }

    bool MatchSession::callRequired() const
    {
        return !m_pushOut && Rules::callRequired(m_frame, objectBallsOnTable());
    }

    void MatchSession::updateAutoCall()
    {
        if (!callRequired())
        {
            m_call.reset();
            return;
        }

        const std::vector<int> onTable = objectBallsOnTable();
        if (onTable.empty())
        {
            m_call.reset();
            return;
        }

        const glm::vec3 cue = m_registry.get<TransformComponent>(m_cueBallEntity).position;
        const glm::vec3 aim = aimDirection();
        const float r = m_variant->table.ballRadius;

        // The ball the aim line meets first, and where the cue ball touches it.
        int hitBall = -1;
        float hitDistance = std::numeric_limits<float>::max();
        for (const int number : onTable)
        {
            const glm::vec3 center = m_registry.get<TransformComponent>(*ballEntity(number)).position;
            const float distance = contactDistance(cue, aim, center, r);
            if ((distance >= 0.0f) && (distance < hitDistance))
            {
                hitDistance = distance;
                hitBall = number;
            }
        }

        Rules::Call call = m_call.value_or(Rules::Call{});
        if (!m_callBallByHand)
        {
            if (m_variant->discipline == GameDiscipline::EightBall)
            {
                call.ball = 8;
            }
            else
            {
                call.ball = (hitBall >= 0) ? hitBall : onTable.front();
            }
        }
        if (std::find(onTable.begin(), onTable.end(), call.ball) == onTable.end())
        {
            call.ball = onTable.front();
        }

        if (!m_callPocketByHand)
        {
            // The pocket best lined up with the called ball's path: the cut
            // direction when the aim hits it, else the line from the cue ball.
            const glm::vec3 ball = m_registry.get<TransformComponent>(*ballEntity(call.ball)).position;
            glm::vec3 travel = ball - cue;
            if (hitBall == call.ball)
            {
                travel = ball - (cue + aim * hitDistance);
            }
            travel.y = 0.0f;
            travel = glm::length(travel) > 1.0e-6f ? glm::normalize(travel) : aim;

            float best = -2.0f;
            for (std::size_t i = 0; i < m_pocketPositions.size(); ++i)
            {
                glm::vec3 toPocket = m_pocketPositions[i] - ball;
                toPocket.y = 0.0f;
                const float alignment = glm::dot(travel, glm::normalize(toPocket));
                if (alignment > best)
                {
                    best = alignment;
                    call.pocket = static_cast<int>(i);
                }
            }
        }

        m_call = call;
    }

    void MatchSession::cycleCalledPocket(int direction)
    {
        if (!m_call || !acceptsShotInput() || m_pocketPositions.empty())
        {
            return;
        }
        const int count = static_cast<int>(m_pocketPositions.size());
        m_call->pocket = ((m_call->pocket + direction) % count + count) % count;
        m_callPocketByHand = true;
    }

    void MatchSession::cycleCalledBall(int direction)
    {
        if (!m_call || !acceptsShotInput() || (m_variant->discipline == GameDiscipline::EightBall))
        {
            return;
        }
        const std::vector<int> onTable = objectBallsOnTable();
        const auto it = std::find(onTable.begin(), onTable.end(), m_call->ball);
        const int count = static_cast<int>(onTable.size());
        const int index = (it == onTable.end()) ? 0 : static_cast<int>(it - onTable.begin());
        m_call->ball = onTable[static_cast<std::size_t>(((index + direction) % count + count) % count)];
        m_callBallByHand = true;
        m_callPocketByHand = false;
        updateAutoCall();
    }

    bool MatchSession::pushOutAvailable() const
    {
        return m_frame.pushOutAvailable && (acceptsShotInput() || placingCueBall());
    }

    void MatchSession::setPushOut(bool declared)
    {
        m_pushOut = declared && pushOutAvailable();
        updateAutoCall();
    }

    // ---- Shooting --------------------------------------------------------------

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

        if (m_shotState.phase == ShotPhase::Aiming)
        {
            updateAutoCall();
        }

        // A hold that began before this turn (e.g. the confirming click) must
        // be released before it can start a shot.
        if ((m_shotState.phase == ShotPhase::Aiming) && holding && !m_holdWasActive)
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
            else
            {
                if (m_chargingByStroke && (m_shotState.charge01 < m_tuning.strokeCancelBelow))
                {
                    m_shotState.phase = ShotPhase::Aiming;
                }
                else
                {
                    const bool fired = fireShot();
                    m_shotState.phase = fired ? ShotPhase::BallsInMotion : ShotPhase::Aiming;
                }

                m_shotState.charge01 = 0.0f;
            }
        }

        m_holdWasActive = holding;
    }

    void MatchSession::cancelHeldShot()
    {
        // Treat any button still down as stale, so the release after resuming
        // (or the click that closed a dialog) does not start or fire a shot.
        m_holdWasActive = true;

        if (m_shotState.phase == ShotPhase::Charging)
        {
            m_shotState.phase = ShotPhase::Aiming;
            m_shotState.charge01 = 0.0f;
        }
    }

    void MatchSession::debugRespotCueBall()
    {
        m_trajectory.reset();
        resetCueBall();
        m_shotState.phase = ShotPhase::Aiming;
        m_shotState.charge01 = 0.0f;
    }

    bool MatchSession::fireShot()
    {
        if (m_registry.get<BallComponent>(m_cueBallEntity).pocketed)
        {
            return false;
        }

        m_onTableAtStrike = objectBallsOnTable();
        m_callAtStrike = callRequired() ? m_call : std::nullopt;
        m_pushOutAtStrike = m_pushOut;

        const TableSpecification& table = m_variant->table;
        const Sim::CueStrike strike = currentStrike(m_shotState.charge01);
        m_lastShotPower = m_shotState.charge01;
        std::vector<Sim::BallState> balls = simBallStates();

        m_trajectory = Sim::simulateShot(m_simTable, std::move(balls), 0, strike, table.simBall);
        m_lastTrajectory = m_trajectory;
        ++m_playbackId;
        m_playbackSeconds = 0.0;
        m_record = Rules::recordShot(*m_trajectory, m_simNumbers);

        applySimStates(m_trajectory->states.front(), 0.0);
        return true;
    }

    std::vector<Sim::BallState> MatchSession::simBallStates() const
    {
        const TableSpecification& table = m_variant->table;
        std::vector<Sim::BallState> balls;
        balls.reserve(m_simBalls.size());
        for (const Entity entity : m_simBalls)
        {
            const BallComponent& ball = m_registry.get<BallComponent>(entity);
            Sim::BallState state;
            state.r = SimBridge::toSimPosition(m_registry.get<TransformComponent>(entity).position, table.clothWidth, table.clothDepth);
            state.r.z = table.simBall.R;
            state.s = ball.pocketed ? Sim::MotionState::Pocketed : Sim::MotionState::Stationary;
            balls.push_back(state);
        }
        return balls;
    }

    Sim::CueStrike MatchSession::currentStrike(float power01) const
    {
        return Sim::CueStrike {
            .speed = m_tuning.minCueSpeed + (m_tuning.maxCueSpeed - m_tuning.minCueSpeed) * power01,
            .phiDegrees = SimBridge::aimToPhiDegrees(aimDirection()),
            .thetaDegrees = 0.0,
            // The simulator's a > 0 is left english; the game's strikeRight01 > 0 is right.
            .a = -m_shotState.strikeRight01 * m_tuning.tipOffsetPerStrikeUnit,
            .b = m_shotState.strikeForward01 * m_tuning.tipOffsetPerStrikeUnit
        };
    }

    const ShotPreview& MatchSession::shotPreview()
    {
        const bool canShoot = acceptsShotInput() && !m_registry.get<BallComponent>(m_cueBallEntity).pocketed;
        if (!canShoot)
        {
            m_preview = ShotPreview{};
            m_previewKey = glm::vec4(-1.0f);
            return m_preview;
        }

        const float power = (m_shotState.phase == ShotPhase::Charging) ? m_shotState.charge01 : m_lastShotPower;
        const glm::vec4 key(m_shotState.aimAngleRadians, m_shotState.strikeRight01, m_shotState.strikeForward01, power);
        const glm::vec3 cue = m_registry.get<TransformComponent>(m_cueBallEntity).position;
        const glm::vec4 change = glm::abs(key - m_previewKey);
        const bool unchanged = m_preview.valid && (glm::length(cue - m_previewCue) < 1.0e-5f) &&
            (change.x < 1.0e-5f) && (change.y < 1.0e-4f) && (change.z < 1.0e-4f) && (change.w < 0.01f);
        if (unchanged)
        {
            return m_preview;
        }
        m_previewKey = key;
        m_previewCue = cue;

        // A capped simulation: the guides only need the first part of the shot.
        Sim::SimulationLimits limits;
        limits.maxEvents = 400;
        const TableSpecification& table = m_variant->table;
        const Sim::ShotTrajectory trajectory =
            Sim::simulateShot(m_simTable, simBallStates(), 0, currentStrike(power), table.simBall, {}, limits);

        ShotPreview preview;
        preview.valid = true;

        int object = -1;
        double contactTime = trajectory.duration();
        for (const Sim::ShotEvent& event : trajectory.events)
        {
            if ((event.type == Sim::EventType::BallBall) && ((event.ball == 0) || (event.other == 0)))
            {
                object = (event.ball == 0) ? event.other : event.ball;
                contactTime = event.time;
                break;
            }
        }

        const auto toGame = [&](const glm::dvec3& r)
        {
            glm::vec3 p = SimBridge::toGamePosition(r, table.clothWidth, table.clothDepth);
            p.y = table.ballRadius;
            return p;
        };

        // Sample both paths at a fixed step, plus the exact contact moment.
        constexpr double SAMPLE_SECONDS = 1.0 / 60.0;
        const double end = trajectory.duration();
        bool contactAdded = false;
        for (double t = 0.0; t <= end + SAMPLE_SECONDS; t += SAMPLE_SECONDS)
        {
            double sampleTime = std::min(t, end);
            if (!contactAdded && (object >= 0) && (sampleTime >= contactTime))
            {
                sampleTime = contactTime;
                contactAdded = true;
                t = contactTime;
            }
            const std::vector<Sim::BallState> states = trajectory.stateAt(sampleTime);

            if (states[0].s != Sim::MotionState::Pocketed)
            {
                preview.cuePath.push_back(toGame(states[0].r));
                if (contactAdded && (preview.cueContactIndex == 0))
                {
                    preview.cueContactIndex = preview.cuePath.size() - 1;
                }
            }
            else
            {
                preview.cuePotted = true;
            }

            if ((object >= 0) && (sampleTime >= contactTime))
            {
                const Sim::BallState& ball = states[static_cast<std::size_t>(object)];
                if (ball.s == Sim::MotionState::Pocketed)
                {
                    preview.objectPotted = true;
                }
                else if (!preview.objectPotted)
                {
                    preview.objectPath.push_back(toGame(ball.r));
                }
            }

            if (sampleTime >= end)
            {
                break;
            }
        }

        if (object >= 0)
        {
            preview.contact = true;
            preview.objectBall = m_simNumbers[static_cast<std::size_t>(object)];
            preview.ghostBall = preview.cuePath.empty() ? cue : preview.cuePath[preview.cueContactIndex];
        }

        m_preview = std::move(preview);
        return m_preview;
    }

    void MatchSession::applySimStates(const std::vector<Sim::BallState>& states, double deltaTimeSeconds)
    {
        const TableSpecification& table = m_variant->table;
        const float dt = static_cast<float>(deltaTimeSeconds);

        for (std::size_t i = 0; i < states.size(); ++i)
        {
            const Sim::BallState& state = states[i];
            TransformComponent& transform = m_registry.get<TransformComponent>(m_simBalls[i]);
            BallComponent& ball = m_registry.get<BallComponent>(m_simBalls[i]);

            transform.syncPrevious();

            if (state.s == Sim::MotionState::Pocketed)
            {
                ball.pocketed = true;
                ball.linearVelocity = glm::vec3(0.0f);
                ball.angularVelocity = glm::vec3(0.0f);
                continue;
            }

            ball.pocketed = false;
            transform.position = SimBridge::toGamePosition(state.r, table.clothWidth, table.clothDepth);
            ball.linearVelocity = SimBridge::toGameVector(state.v);
            ball.angularVelocity = SimBridge::toGameVector(state.w);

            // Turn the ball by its spin so the numbers roll naturally.
            const float spin = glm::length(ball.angularVelocity);
            if ((spin > 1.0e-6f) && (dt > 0.0f))
            {
                const glm::quat turn = glm::angleAxis(spin * dt, ball.angularVelocity / spin);
                transform.rotation = glm::normalize(turn * transform.rotation);
            }
        }
    }

    void MatchSession::playBack(double deltaTimeSeconds)
    {
        if (!m_trajectory)
        {
            return;
        }

        m_playbackSeconds += deltaTimeSeconds;
        const double end = m_trajectory->duration();
        applySimStates(m_trajectory->stateAt(std::min(m_playbackSeconds, end)), deltaTimeSeconds);

        if (m_playbackSeconds >= end)
        {
            m_trajectory.reset();   // the table is at rest
        }
    }

    const Sim::ShotTrajectory* MatchSession::playbackTrajectory() const
    {
        if (m_replay && m_lastTrajectory)
        {
            return &*m_lastTrajectory;
        }
        return activeTrajectory();
    }

    bool MatchSession::canReplay() const
    {
        const bool betweenShots = frameOver() ||
            (m_frame.choice != Rules::Choice::None) ||
            (m_shotState.phase == ShotPhase::Aiming) || (m_shotState.phase == ShotPhase::PlacingCueBall);
        return m_lastTrajectory.has_value() && !m_trajectory && !m_replay && betweenShots;
    }

    bool MatchSession::startReplay(double speed)
    {
        if (!canReplay())
        {
            return false;
        }

        Replay replay;
        replay.speed = std::max(speed, 0.05);
        for (const Entity entity : m_simBalls)
        {
            replay.table.push_back({m_registry.get<TransformComponent>(entity), m_registry.get<BallComponent>(entity).pocketed});
        }
        m_replay = std::move(replay);
        ++m_playbackId;
        m_shotState.charge01 = 0.0f;
        applySimStates(m_lastTrajectory->states.front(), 0.0);
        return true;
    }

    void MatchSession::stopReplay()
    {
        if (!m_replay)
        {
            return;
        }
        for (std::size_t i = 0; i < m_simBalls.size(); ++i)
        {
            const BallSnapshot& snapshot = m_replay->table[i];
            m_registry.get<TransformComponent>(m_simBalls[i]) = snapshot.transform;
            m_registry.get<TransformComponent>(m_simBalls[i]).syncPrevious();
            BallComponent& ball = m_registry.get<BallComponent>(m_simBalls[i]);
            ball.pocketed = snapshot.pocketed;
            ball.linearVelocity = glm::vec3(0.0f);
            ball.angularVelocity = glm::vec3(0.0f);
        }
        m_replay.reset();
        m_holdWasActive = true;   // the key that ended the replay must not start a stroke
    }

    void MatchSession::resetShotClock()
    {
        m_clock.enabled = m_settings.shotClockSeconds > 0.0f;
        m_clock.remaining = m_settings.shotClockSeconds;
        m_clock.running = false;
    }

    bool MatchSession::useExtension()
    {
        const std::size_t shooter = static_cast<std::size_t>(m_frame.shooter);
        if (!m_clock.enabled || !m_clock.running || !m_clock.extensionAvailable[shooter])
        {
            return false;
        }
        m_clock.extensionAvailable[shooter] = false;
        m_clock.remaining += m_settings.extensionSeconds;
        return true;
    }

    void MatchSession::tickShotClock(double deltaTimeSeconds)
    {
        const bool shooterUp = acceptsShotInput() || placingCueBall();
        m_clock.running = m_clock.enabled && shooterUp && (m_frame.phase != Rules::Phase::Break);
        if (!m_clock.running)
        {
            return;
        }

        m_clock.remaining -= static_cast<float>(deltaTimeSeconds);
        if (m_clock.remaining > 0.0f)
        {
            return;
        }

        // Time foul: no shot is played; the referee calls it and play goes on.
        m_clock.remaining = 0.0f;
        ShotOutcome outcome;
        outcome.verdict = Rules::judgeTimeFoul(m_frame);
        if (outcome.verdict.frameOver)
        {
            m_nextBreaker = m_score.recordFrame(outcome.verdict.winner, m_frame.breaker);
        }
        outcome.nextPlayer = m_frame.shooter;
        outcome.matchOver = m_score.over();
        m_lastOutcome = std::move(outcome);
        ++m_resolvedShotCount;
        m_holdWasActive = true;
        enterTurn();
    }

    void MatchSession::step(double deltaTimeSeconds)
    {
        if (!m_replay)
        {
            tickShotClock(deltaTimeSeconds);
        }

        if (m_replay)
        {
            // Hold the final frame a moment before handing the table back.
            constexpr double HOLD_SECONDS = 0.6;
            const double step = deltaTimeSeconds * m_replay->speed;
            m_replay->seconds += step;
            const double end = m_lastTrajectory->duration();
            applySimStates(m_lastTrajectory->stateAt(std::min(m_replay->seconds, end)), step);
            if (m_replay->seconds >= end + HOLD_SECONDS * m_replay->speed)
            {
                stopReplay();
            }
            return;
        }

        playBack(deltaTimeSeconds);

        if ((m_shotState.phase == ShotPhase::BallsInMotion) && !ballsInMotion())
        {
            resolveShot();
        }
    }

    void MatchSession::resolveShot()
    {
        m_record.pushOut = m_pushOutAtStrike;
        m_record.call = m_callAtStrike;

        ShotOutcome outcome;
        outcome.verdict = Rules::judgeShot(m_frame, m_onTableAtStrike, m_record);

        for (const int number : outcome.verdict.spot)
        {
            spotBall(number);
        }

        if (outcome.verdict.frameOver)
        {
            m_nextBreaker = m_score.recordFrame(outcome.verdict.winner, m_frame.breaker);
        }

        outcome.nextPlayer = m_frame.shooter;
        outcome.matchOver = m_score.over();
        m_lastOutcome = std::move(outcome);
        ++m_resolvedShotCount;

        enterTurn();
    }
}
