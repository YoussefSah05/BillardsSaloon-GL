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
            constexpr float SLIP_EPSILON = 0.01f;
            constexpr float SPIN_EPSILON = 0.01f;
            constexpr float CONTACT_TANGENT_SPEED_EPSILON = 0.0005f;

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
                        ref.transform->position.x = -maxX;
                        ref.ball->linearVelocity.x = -ref.ball->linearVelocity.x * tableBounds->railRestitution;
                    }
                    else if (ref.transform->position.x > maxX)
                    {
                        ref.transform->position.x = maxX;
                        ref.ball->linearVelocity.x = -ref.ball->linearVelocity.x * tableBounds->railRestitution;
                    }

                    if (ref.transform->position.z < -maxZ)
                    {
                        ref.transform->position.z = -maxZ;
                        ref.ball->linearVelocity.z = -ref.ball->linearVelocity.z * tableBounds->railRestitution;
                    }
                    else if (ref.transform->position.z > maxZ)
                    {
                        ref.transform->position.z = maxZ;
                        ref.ball->linearVelocity.z = -ref.ball->linearVelocity.z * tableBounds->railRestitution;
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
                    : 0.06f;

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
                        const glm::vec3 raCrossTangent = glm::cross(contactOffsetA, tangent);
                        const glm::vec3 rbCrossTangent = glm::cross(contactOffsetB, tangent);

                        const float inverseInertiaA = 1.0f / solidSphereInertia(*a.ball);
                        const float inverseInertiaB = 1.0f / solidSphereInertia(*b.ball);

                        const float tangentDenominator =
                            inverseMassSum +
                            inverseInertiaA * glm::dot(raCrossTangent, raCrossTangent) +
                            inverseInertiaB * glm::dot(rbCrossTangent, rbCrossTangent);

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
                    : 0.020f;

                const float stopThreshold = (tableBounds != nullptr)
                    ? tableBounds->stopSpeedThreshold
                    : 0.02f;

                const float sideSpinDamping = (tableBounds != nullptr)
                    ? tableBounds->sideSpinDampingPerSecond
                    : 0.35f;

                for (BallRef& ref : balls)
                {
                    if (ref.ball->pocketed)
                    {
                        continue;
                    }

                    glm::vec2 v = planarXZ(ref.ball->linearVelocity);
                    glm::vec2 slip = contactSlipVelocity(*ref.ball);
                    const float slipSpeed = glm::length(slip);

                    if (slipSpeed > SLIP_EPSILON)
                    {
                        const glm::vec2 slipDir = slip / slipSpeed;
                        const glm::vec2 acceleration = -muSlide * GRAVITY * slipDir;

                        v += acceleration * dt;

                        ref.ball->angularVelocity.x +=
                            (-5.0f * acceleration.y / (2.0f * ref.ball->radius)) * dt;
                        ref.ball->angularVelocity.z +=
                            ( 5.0f * acceleration.x / (2.0f * ref.ball->radius)) * dt;
                    }
                    else
                    {
                        const float speed = glm::length(v);

                        if (speed > 0.0f)
                        {
                            const glm::vec2 dir = v / speed;
                            const float newSpeed = std::max(0.0f, speed - muRoll * GRAVITY * dt);
                            v = dir * newSpeed;
                        }

                        ref.ball->angularVelocity.x = v.y / ref.ball->radius;
                        ref.ball->angularVelocity.z = -v.x / ref.ball->radius;
                    }

                    ref.ball->angularVelocity.y *= std::max(0.0f, 1.0f - sideSpinDamping * dt);
                    ref.ball->linearVelocity.x = v.x;
                    ref.ball->linearVelocity.z = v.y;
                    ref.ball->linearVelocity.y = 0.0f;

                    const float speed = glm::length(v);
                    if ((speed < stopThreshold) &&
                        (std::abs(ref.ball->angularVelocity.y) < SPIN_EPSILON) &&
                        (glm::length(glm::vec2(ref.ball->angularVelocity.x, ref.ball->angularVelocity.z)) < SPIN_EPSILON))
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