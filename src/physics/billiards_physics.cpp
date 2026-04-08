#include "physics/billiards_physics.h"

#include "scene/components.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace BilliardsSaloon
{
    namespace Physics
    {
        namespace
        {
            constexpr float GRAVITY = 9.81f;
            constexpr float POSITIONAL_CORRECTION_PERCENT = 0.8f;
            constexpr float POSITIONAL_CORRECTION_SLOP = 0.0001f;
            constexpr float SLIP_EPSILON = 0.0015f;
            constexpr float SPIN_EPSILON = 0.01f;
            constexpr float CONTACT_TANGENT_SPEED_EPSILON = 0.0005f;
            constexpr float MOTION_STATE_TIME_EPSILON = 1.0e-6f;
            constexpr int MAX_CLOTH_STATE_STEPS = 4;

            struct BallRef
            {
                Entity entity {};
                TransformComponent* transform {nullptr};
                BallComponent* ball {nullptr};
            };

            [[nodiscard]] TableBoundsComponent* findTableBounds(Registry& registry)
            {
                TableBoundsComponent* result = nullptr;

                registry.view<TableBoundsComponent>().each(
                    [&](Entity, TableBoundsComponent& bounds)
                    {
                        if (result == nullptr)
                        {
                            result = &bounds;
                        }
                    }
                );

                return result;
            }

            [[nodiscard]] std::vector<BallRef> collectBalls(Registry& registry)
            {
                std::vector<BallRef> balls;

                registry.view<TransformComponent, BallComponent>().each(
                    [&](Entity entity, TransformComponent& transform, BallComponent& ball)
                    {
                        balls.push_back(BallRef{
                            .entity = entity,
                            .transform = &transform,
                            .ball = &ball
                        });
                    }
                );

                return balls;
            }

            [[nodiscard]] glm::vec2 planarXZ(const glm::vec3& v)
            {
                return glm::vec2(v.x, v.z);
            }

            [[nodiscard]] glm::vec3 fromPlanarXZ(const glm::vec2& v, float y = 0.0f)
            {
                return glm::vec3(v.x, y, v.y);
            }

            [[nodiscard]] glm::vec2 contactSlipVelocity(const BallComponent& ball)
            {
                return glm::vec2(
                    ball.linearVelocity.x + ball.radius * ball.angularVelocity.z,
                    ball.linearVelocity.z - ball.radius * ball.angularVelocity.x
                );
            }

            [[nodiscard]] float solidSphereInertia(const BallComponent& ball)
            {
                return 0.4f * ball.massKg * ball.radius * ball.radius;
            }

            [[nodiscard]] glm::vec3 contactPointVelocity(
                const BallComponent& ball,
                const glm::vec3& contactOffset)
            {
                return ball.linearVelocity + glm::cross(ball.angularVelocity, contactOffset);
            }

            void applyImpulseAtOffset(
                BallComponent& ball,
                const glm::vec3& impulse,
                const glm::vec3& contactOffset)
            {
                ball.linearVelocity += impulse / ball.massKg;

                const float inertia = solidSphereInertia(ball);
                if (inertia > 0.0f)
                {
                    ball.angularVelocity += glm::cross(contactOffset, impulse) / inertia;
                }
            }

            [[nodiscard]] float impulseDenominatorForDirection(
                const BallComponent& ball,
                const glm::vec3& contactOffset,
                const glm::vec3& direction)
            {
                const float inverseMass = 1.0f / ball.massKg;
                const float inertia = solidSphereInertia(ball);

                if (inertia <= 0.0f)
                {
                    return inverseMass;
                }

                const glm::vec3 crossTerm = glm::cross(contactOffset, direction);
                return inverseMass + glm::dot(crossTerm, crossTerm) / inertia;
            }

            [[nodiscard]] glm::vec3 railTangentFromNormal(const glm::vec3& railNormal)
            {
                return (std::abs(railNormal.x) > 0.5f)
                    ? glm::vec3(0.0f, 0.0f, 1.0f)
                    : glm::vec3(1.0f, 0.0f, 0.0f);
            }

            void setRollingAngularVelocityFromLinear(BallComponent& ball)
            {
                if (ball.radius <= 0.0f)
                {
                    ball.angularVelocity.x = 0.0f;
                    ball.angularVelocity.z = 0.0f;
                    return;
                }

                ball.angularVelocity.x = ball.linearVelocity.z / ball.radius;
                ball.angularVelocity.z = -ball.linearVelocity.x / ball.radius;
            }

            void decayVerticalSpin(BallComponent& ball, float dt, float spinningFrictionCoefficient)
            {
                if ((dt <= 0.0f) || (spinningFrictionCoefficient <= 0.0f) || (ball.radius <= 0.0f))
                {
                    return;
                }

                const float spinDeceleration =
                    spinningFrictionCoefficient * GRAVITY / ball.radius;

                const float deltaSpin = spinDeceleration * dt;
                const float spin = ball.angularVelocity.y;

                if (std::abs(spin) <= deltaSpin)
                {
                    ball.angularVelocity.y = 0.0f;
                    return;
                }

                ball.angularVelocity.y = spin - std::copysign(deltaSpin, spin);
            }

            [[nodiscard]] float evolveSlidingMotion(
                BallComponent& ball,
                float dt,
                float slidingFrictionCoefficient,
                float spinningFrictionCoefficient)
            {
                const glm::vec2 slip = contactSlipVelocity(ball);
                const float slipSpeed = glm::length(slip);

                if ((slipSpeed <= SLIP_EPSILON) || (slidingFrictionCoefficient <= 0.0f))
                {
                    setRollingAngularVelocityFromLinear(ball);
                    return dt;
                }

                const float timeToRolling =
                    (2.0f * slipSpeed) / (7.0f * slidingFrictionCoefficient * GRAVITY);

                const float evolveTime = std::min(dt, timeToRolling);
                const glm::vec2 slipDirection = slip / slipSpeed;

                const glm::vec2 linearAcceleration =
                    -slidingFrictionCoefficient * GRAVITY * slipDirection;

                const glm::vec2 newPlanarVelocity =
                    planarXZ(ball.linearVelocity) + linearAcceleration * evolveTime;

                ball.linearVelocity = fromPlanarXZ(newPlanarVelocity);

                const float angularAccelerationScale =
                    (5.0f * slidingFrictionCoefficient * GRAVITY) / (2.0f * ball.radius);

                ball.angularVelocity.x += angularAccelerationScale * slipDirection.y * evolveTime;
                ball.angularVelocity.z -= angularAccelerationScale * slipDirection.x * evolveTime;

                decayVerticalSpin(ball, evolveTime, spinningFrictionCoefficient);

                const float remainingTime = dt - evolveTime;
                if (remainingTime > MOTION_STATE_TIME_EPSILON)
                {
                    setRollingAngularVelocityFromLinear(ball);
                }

                return remainingTime;
            }

            [[nodiscard]] float evolveRollingMotion(
                BallComponent& ball,
                float dt,
                float rollingFrictionCoefficient,
                float spinningFrictionCoefficient,
                float stopSpeedThreshold)
            {
                const glm::vec2 planarVelocity = planarXZ(ball.linearVelocity);
                const float speed = glm::length(planarVelocity);

                if (speed <= stopSpeedThreshold)
                {
                    ball.linearVelocity = glm::vec3(0.0f);
                    ball.angularVelocity.x = 0.0f;
                    ball.angularVelocity.z = 0.0f;
                    return dt;
                }

                if (rollingFrictionCoefficient <= 0.0f)
                {
                    setRollingAngularVelocityFromLinear(ball);
                    decayVerticalSpin(ball, dt, spinningFrictionCoefficient);
                    return 0.0f;
                }

                const float rollingDeceleration = rollingFrictionCoefficient * GRAVITY;
                const float timeToStop = speed / rollingDeceleration;
                const float evolveTime = std::min(dt, timeToStop);

                const glm::vec2 direction = planarVelocity / speed;
                const float newSpeed = std::max(0.0f, speed - rollingDeceleration * evolveTime);
                ball.linearVelocity = fromPlanarXZ(direction * newSpeed);
                setRollingAngularVelocityFromLinear(ball);
                decayVerticalSpin(ball, evolveTime, spinningFrictionCoefficient);

                if (newSpeed <= stopSpeedThreshold)
                {
                    ball.linearVelocity = glm::vec3(0.0f);
                    ball.angularVelocity.x = 0.0f;
                    ball.angularVelocity.z = 0.0f;
                }

                return dt - evolveTime;
            }

            void evolveSpinningMotion(
                BallComponent& ball,
                float dt,
                float spinningFrictionCoefficient)
            {
                ball.linearVelocity = glm::vec3(0.0f);
                ball.angularVelocity.x = 0.0f;
                ball.angularVelocity.z = 0.0f;
                decayVerticalSpin(ball, dt, spinningFrictionCoefficient);

                if (std::abs(ball.angularVelocity.y) < SPIN_EPSILON)
                {
                    ball.angularVelocity.y = 0.0f;
                }
            }

            void resolveSingleRailContact(
                BallRef& ref,
                const glm::vec3& railNormal,
                float boundaryCoordinate,
                float restitution,
                float frictionCoefficient)
            {
                if (std::abs(railNormal.x) > 0.5f)
                {
                    ref.transform->position.x = boundaryCoordinate;
                }
                else
                {
                    ref.transform->position.z = boundaryCoordinate;
                }

                const glm::vec3 contactOffset = -railNormal * ref.ball->radius;

                glm::vec3 contactVelocity =
                    contactPointVelocity(*ref.ball, contactOffset);
                contactVelocity.y = 0.0f;

                const float normalSpeed = glm::dot(contactVelocity, railNormal);
                if (normalSpeed >= 0.0f)
                {
                    return;
                }

                const float normalDenominator =
                    impulseDenominatorForDirection(*ref.ball, contactOffset, railNormal);

                if (normalDenominator <= 0.0f)
                {
                    return;
                }

                const float normalImpulseMagnitude =
                    -(1.0f + restitution) * normalSpeed / normalDenominator;

                applyImpulseAtOffset(
                    *ref.ball,
                    normalImpulseMagnitude * railNormal,
                    contactOffset
                );

                const glm::vec3 tangent = railTangentFromNormal(railNormal);

                contactVelocity = contactPointVelocity(*ref.ball, contactOffset);
                contactVelocity.y = 0.0f;

                const float tangentSpeed = glm::dot(contactVelocity, tangent);
                if (std::abs(tangentSpeed) < CONTACT_TANGENT_SPEED_EPSILON)
                {
                    return;
                }

                const float tangentDenominator =
                    impulseDenominatorForDirection(*ref.ball, contactOffset, tangent);

                if (tangentDenominator <= 0.0f)
                {
                    return;
                }

                float tangentialImpulseMagnitude = -tangentSpeed / tangentDenominator;
                const float maxTangentialImpulse =
                    frictionCoefficient * normalImpulseMagnitude;

                tangentialImpulseMagnitude = std::clamp(
                    tangentialImpulseMagnitude,
                    -maxTangentialImpulse,
                    maxTangentialImpulse
                );

                applyImpulseAtOffset(
                    *ref.ball,
                    tangentialImpulseMagnitude * tangent,
                    contactOffset
                );

                ref.ball->linearVelocity.y = 0.0f;
            }

            void integratePositions(std::vector<BallRef>& balls, float dt)
            {
                for (BallRef& ref : balls)
                {
                    if (ref.ball->pocketed)
                    {
                        continue;
                    }

                    ref.transform->syncPrevious();
                    ref.transform->position += ref.ball->linearVelocity * dt;
                }
            }

            void captureBallIntoPocket(
                BallRef& ref,
                const glm::vec2& pocketCenter,
                ShotResult& shotResult)
            {
                ref.ball->pocketed = true;
                ref.ball->linearVelocity = glm::vec3(0.0f);
                ref.ball->angularVelocity = glm::vec3(0.0f);

                ref.transform->previousPosition = ref.transform->position;
                ref.transform->position = glm::vec3(pocketCenter.x, -0.20f, pocketCenter.y);

                shotResult.pocketedBalls.push_back(PocketedBallRecord{
                    .number = ref.ball->number,
                    .ruleTag = ref.ball->ruleTag,
                    .isCueBall = ref.ball->isCueBall
                });

                if (ref.ball->isCueBall)
                {
                    shotResult.cueBallPocketed = true;
                }
            }

            void resolvePocketCaptures(
                std::vector<BallRef>& balls,
                const TableBoundsComponent* tableBounds,
                ShotResult& shotResult)
            {
                if (tableBounds == nullptr)
                {
                    return;
                }

                const glm::vec2 cornerPockets[4] = {
                    {-tableBounds->halfWidth, -tableBounds->halfDepth},
                    { tableBounds->halfWidth, -tableBounds->halfDepth},
                    {-tableBounds->halfWidth,  tableBounds->halfDepth},
                    { tableBounds->halfWidth,  tableBounds->halfDepth}
                };

                const glm::vec2 sidePockets[2] = {
                    {0.0f, -tableBounds->halfDepth},
                    {0.0f,  tableBounds->halfDepth}
                };

                for (BallRef& ref : balls)
                {
                    if (ref.ball->pocketed)
                    {
                        continue;
                    }

                    const glm::vec2 positionXZ(
                        ref.transform->position.x,
                        ref.transform->position.z
                    );

                    bool captured = false;

                    for (const glm::vec2& pocket : cornerPockets)
                    {
                        if (glm::length(positionXZ - pocket) <= tableBounds->cornerPocketRadius)
                        {
                            captureBallIntoPocket(ref, pocket, shotResult);
                            captured = true;
                            break;
                        }
                    }

                    if (captured)
                    {
                        continue;
                    }

                    for (const glm::vec2& pocket : sidePockets)
                    {
                        if (glm::length(positionXZ - pocket) <= tableBounds->sidePocketRadius)
                        {
                            captureBallIntoPocket(ref, pocket, shotResult);
                            break;
                        }
                    }
                }
            }

            void resolveRailCollisions(
                std::vector<BallRef>& balls,
                const TableBoundsComponent* tableBounds)
            {
                if (tableBounds == nullptr)
                {
                    return;
                }

                const float restitution = tableBounds->railRestitution;
                const float frictionCoefficient = tableBounds->railContactFrictionCoefficient;

                for (BallRef& ref : balls)
                {
                    if (ref.ball->pocketed)
                    {
                        continue;
                    }

                    const float maxX = tableBounds->halfWidth - ref.ball->radius;
                    const float maxZ = tableBounds->halfDepth - ref.ball->radius;

                    if (ref.transform->position.x < -maxX)
                    {
                        resolveSingleRailContact(
                            ref,
                            glm::vec3(1.0f, 0.0f, 0.0f),
                            -maxX,
                            restitution,
                            frictionCoefficient
                        );
                    }
                    else if (ref.transform->position.x > maxX)
                    {
                        resolveSingleRailContact(
                            ref,
                            glm::vec3(-1.0f, 0.0f, 0.0f),
                            maxX,
                            restitution,
                            frictionCoefficient
                        );
                    }

                    if (ref.transform->position.z < -maxZ)
                    {
                        resolveSingleRailContact(
                            ref,
                            glm::vec3(0.0f, 0.0f, 1.0f),
                            -maxZ,
                            restitution,
                            frictionCoefficient
                        );
                    }
                    else if (ref.transform->position.z > maxZ)
                    {
                        resolveSingleRailContact(
                            ref,
                            glm::vec3(0.0f, 0.0f, -1.0f),
                            maxZ,
                            restitution,
                            frictionCoefficient
                        );
                    }
                }
            }

            void resolveBallBallCollisions(
                std::vector<BallRef>& balls,
                const TableBoundsComponent* tableBounds,
                ShotResult& shotResult)
            {
                const float restitution = (tableBounds != nullptr)
                    ? tableBounds->ballRestitution
                    : 0.96f;

                const float contactFrictionCoefficient = (tableBounds != nullptr)
                    ? tableBounds->ballContactFrictionCoefficient
                    : 0.05f;

                for (std::size_t i = 0; i < balls.size(); ++i)
                {
                    for (std::size_t j = i + 1; j < balls.size(); ++j)
                    {
                        BallRef& a = balls[i];
                        BallRef& b = balls[j];

                        if (a.ball->pocketed || b.ball->pocketed)
                        {
                            continue;
                        }

                        glm::vec3 delta = b.transform->position - a.transform->position;
                        delta.y = 0.0f;

                        const float distanceSquared = glm::dot(delta, delta);
                        const float combinedRadius = a.ball->radius + b.ball->radius;
                        const float combinedRadiusSquared = combinedRadius * combinedRadius;

                        if (distanceSquared > combinedRadiusSquared)
                        {
                            continue;
                        }

                        float distance = std::sqrt(distanceSquared);
                        glm::vec3 normal(1.0f, 0.0f, 0.0f);

                        if (distance > 1.0e-6f)
                        {
                            normal = delta / distance;
                        }

                        if (shotResult.firstObjectBallNumber < 0)
                        {
                            if (a.ball->isCueBall && !b.ball->isCueBall)
                            {
                                shotResult.firstObjectBallNumber = b.ball->number;
                                shotResult.firstObjectBallTag = b.ball->ruleTag;
                            }
                            else if (b.ball->isCueBall && !a.ball->isCueBall)
                            {
                                shotResult.firstObjectBallNumber = a.ball->number;
                                shotResult.firstObjectBallTag = a.ball->ruleTag;
                            }
                        }

                        const float penetration = combinedRadius - distance;
                        const float inverseMassA = 1.0f / a.ball->massKg;
                        const float inverseMassB = 1.0f / b.ball->massKg;
                        const float inverseMassSum = inverseMassA + inverseMassB;

                        if (penetration > POSITIONAL_CORRECTION_SLOP)
                        {
                            const float correctionMagnitude =
                                ((penetration - POSITIONAL_CORRECTION_SLOP) / inverseMassSum) *
                                POSITIONAL_CORRECTION_PERCENT;

                            const glm::vec3 correction = correctionMagnitude * normal;

                            a.transform->position -= inverseMassA * correction;
                            b.transform->position += inverseMassB * correction;
                        }

                        const glm::vec3 contactOffsetA = normal * a.ball->radius;
                        const glm::vec3 contactOffsetB = -normal * b.ball->radius;

                        glm::vec3 relativeVelocity =
                            contactPointVelocity(*b.ball, contactOffsetB) -
                            contactPointVelocity(*a.ball, contactOffsetA);
                        relativeVelocity.y = 0.0f;

                        const float normalSpeed = glm::dot(relativeVelocity, normal);

                        if (normalSpeed > 0.0f)
                        {
                            continue;
                        }

                        const float normalImpulseMagnitude =
                            -(1.0f + restitution) * normalSpeed / inverseMassSum;

                        const glm::vec3 normalImpulse = normalImpulseMagnitude * normal;

                        applyImpulseAtOffset(*a.ball, -normalImpulse, contactOffsetA);
                        applyImpulseAtOffset(*b.ball,  normalImpulse, contactOffsetB);

                        relativeVelocity =
                            contactPointVelocity(*b.ball, contactOffsetB) -
                            contactPointVelocity(*a.ball, contactOffsetA);
                        relativeVelocity.y = 0.0f;

                        const glm::vec3 tangentialVelocity =
                            relativeVelocity - glm::dot(relativeVelocity, normal) * normal;
                        const float tangentialSpeed = glm::length(tangentialVelocity);

                        if (tangentialSpeed < CONTACT_TANGENT_SPEED_EPSILON)
                        {
                            continue;
                        }

                        const glm::vec3 tangent = tangentialVelocity / tangentialSpeed;

                        const float tangentDenominator =
                            impulseDenominatorForDirection(*a.ball, contactOffsetA, tangent) +
                            impulseDenominatorForDirection(*b.ball, contactOffsetB, tangent);

                        if (tangentDenominator <= 0.0f)
                        {
                            continue;
                        }

                        float tangentialImpulseMagnitude =
                            -glm::dot(relativeVelocity, tangent) / tangentDenominator;

                        const float maxFrictionImpulse =
                            contactFrictionCoefficient * normalImpulseMagnitude;

                        tangentialImpulseMagnitude = std::clamp(
                            tangentialImpulseMagnitude,
                            -maxFrictionImpulse,
                            maxFrictionImpulse
                        );

                        const glm::vec3 tangentialImpulse = tangentialImpulseMagnitude * tangent;

                        applyImpulseAtOffset(*a.ball, -tangentialImpulse, contactOffsetA);
                        applyImpulseAtOffset(*b.ball,  tangentialImpulse, contactOffsetB);
                    }
                }
            }

            void applyClothContactModel(
                std::vector<BallRef>& balls,
                const TableBoundsComponent* tableBounds,
                float dt)
            {
                const float muSlide = (tableBounds != nullptr)
                    ? tableBounds->slidingFrictionCoefficient
                    : 0.20f;

                const float muRoll = (tableBounds != nullptr)
                    ? tableBounds->rollingFrictionCoefficient
                    : 0.010f;

                const float muSpin = (tableBounds != nullptr)
                    ? tableBounds->spinningFrictionCoefficient
                    : 0.015f;

                const float stopThreshold = (tableBounds != nullptr)
                    ? tableBounds->stopSpeedThreshold
                    : 0.006f;

                for (BallRef& ref : balls)
                {
                    if (ref.ball->pocketed)
                    {
                        continue;
                    }

                    float remainingTime = dt;

                    for (int stateStep = 0;
                         (stateStep < MAX_CLOTH_STATE_STEPS) && (remainingTime > MOTION_STATE_TIME_EPSILON);
                         ++stateStep)
                    {
                        const float slipSpeed = glm::length(contactSlipVelocity(*ref.ball));
                        const float planarSpeed = glm::length(planarXZ(ref.ball->linearVelocity));
                        const float verticalSpinMagnitude = std::abs(ref.ball->angularVelocity.y);

                        if (slipSpeed > SLIP_EPSILON)
                        {
                            remainingTime = evolveSlidingMotion(
                                *ref.ball,
                                remainingTime,
                                muSlide,
                                muSpin
                            );
                            continue;
                        }

                        if (planarSpeed > stopThreshold)
                        {
                            setRollingAngularVelocityFromLinear(*ref.ball);
                            remainingTime = evolveRollingMotion(
                                *ref.ball,
                                remainingTime,
                                muRoll,
                                muSpin,
                                stopThreshold
                            );
                            continue;
                        }

                        if (verticalSpinMagnitude > SPIN_EPSILON)
                        {
                            evolveSpinningMotion(*ref.ball, remainingTime, muSpin);
                            remainingTime = 0.0f;
                            continue;
                        }

                        ref.ball->linearVelocity = glm::vec3(0.0f);
                        ref.ball->angularVelocity = glm::vec3(0.0f);
                        remainingTime = 0.0f;
                    }

                    ref.ball->linearVelocity.y = 0.0f;

                    const float finalPlanarSpeed = glm::length(planarXZ(ref.ball->linearVelocity));
                    if ((finalPlanarSpeed <= stopThreshold) &&
                        (std::abs(ref.ball->angularVelocity.y) <= SPIN_EPSILON) &&
                        (glm::length(glm::vec2(ref.ball->angularVelocity.x, ref.ball->angularVelocity.z)) <= SPIN_EPSILON))
                    {
                        ref.ball->linearVelocity = glm::vec3(0.0f);
                        ref.ball->angularVelocity = glm::vec3(0.0f);
                    }
                }
            }

            void updateVisualRotation(std::vector<BallRef>& balls, float dt)
            {
                for (BallRef& ref : balls)
                {
                    if (ref.ball->pocketed)
                    {
                        continue;
                    }

                    const float omegaMagnitude = glm::length(ref.ball->angularVelocity);
                    if (omegaMagnitude < 1.0e-5f)
                    {
                        continue;
                    }

                    const glm::vec3 axis = ref.ball->angularVelocity / omegaMagnitude;
                    const float deltaAngle = omegaMagnitude * dt;

                    const glm::quat deltaRotation = glm::angleAxis(deltaAngle, axis);
                    ref.transform->rotation = glm::normalize(deltaRotation * ref.transform->rotation);
                }
            }
        }

        void stepBilliardsWorld(
            Registry& registry,
            double deltaTimeSeconds,
            ShotResult& shotResult)
        {
            const float dt = static_cast<float>(deltaTimeSeconds);

            TableBoundsComponent* tableBounds = findTableBounds(registry);
            std::vector<BallRef> balls = collectBalls(registry);

            if (balls.empty())
            {
                return;
            }

            integratePositions(balls, dt);
            resolvePocketCaptures(balls, tableBounds, shotResult);
            resolveRailCollisions(balls, tableBounds);

            constexpr int SOLVER_ITERATIONS = 4;
            for (int iteration = 0; iteration < SOLVER_ITERATIONS; ++iteration)
            {
                resolveBallBallCollisions(balls, tableBounds, shotResult);
                resolvePocketCaptures(balls, tableBounds, shotResult);
                resolveRailCollisions(balls, tableBounds);
            }

            applyClothContactModel(balls, tableBounds, dt);
            updateVisualRotation(balls, dt);
        }

        bool anyBallInMotion(Registry& registry)
        {
            bool moving = false;

            registry.view<BallComponent>().each(
                [&](Entity, BallComponent& ball)
                {
                    if (ball.pocketed)
                    {
                        return;
                    }

                    glm::vec3 planarVelocity = ball.linearVelocity;
                    planarVelocity.y = 0.0f;

                    if ((glm::length(planarVelocity) > 0.0005f) ||
                        (glm::length(ball.angularVelocity) > 0.0005f))
                    {
                        moving = true;
                    }
                }
            );

            return moving;
        }
    }
}