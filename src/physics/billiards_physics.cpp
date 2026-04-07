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

            void applyRollingFriction(
                std::vector<BallRef>& balls,
                const TableBoundsComponent* tableBounds,
                float dt)
            {
                const float mu = (tableBounds != nullptr)
                    ? tableBounds->rollingFrictionCoefficient
                    : 0.020f;

                const float stopThreshold = (tableBounds != nullptr)
                    ? tableBounds->stopSpeedThreshold
                    : 0.02f;

                const float decelerationMagnitude = mu * GRAVITY;

                for (BallRef& ref : balls)
                {
                    if (ref.ball->pocketed)
                    {
                        continue;
                    }

                    glm::vec3 planarVelocity = ref.ball->linearVelocity;
                    planarVelocity.y = 0.0f;

                    const float speed = glm::length(planarVelocity);
                    if (speed <= 0.0f)
                    {
                        ref.ball->linearVelocity = glm::vec3(0.0f);
                        ref.ball->angularVelocity = glm::vec3(0.0f);
                        continue;
                    }

                    const float speedDrop = decelerationMagnitude * dt;
                    const float newSpeed = std::max(0.0f, speed - speedDrop);

                    if (newSpeed <= stopThreshold)
                    {
                        ref.ball->linearVelocity = glm::vec3(0.0f);
                        ref.ball->angularVelocity = glm::vec3(0.0f);
                        continue;
                    }

                    const glm::vec3 direction = planarVelocity / speed;
                    ref.ball->linearVelocity = direction * newSpeed;

                    const glm::vec3 rollAxis = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), direction);
                    ref.ball->angularVelocity = rollAxis * (newSpeed / ref.ball->radius);
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

                        if (penetration > POSITIONAL_CORRECTION_SLOP)
                        {
                            const float inverseMassA = 1.0f / a.ball->massKg;
                            const float inverseMassB = 1.0f / b.ball->massKg;
                            const float inverseMassSum = inverseMassA + inverseMassB;

                            const float correctionMagnitude =
                                ((penetration - POSITIONAL_CORRECTION_SLOP) / inverseMassSum) *
                                POSITIONAL_CORRECTION_PERCENT;

                            const glm::vec3 correction = correctionMagnitude * normal;

                            a.transform->position -= inverseMassA * correction;
                            b.transform->position += inverseMassB * correction;
                        }

                        glm::vec3 relativeVelocity = b.ball->linearVelocity - a.ball->linearVelocity;
                        relativeVelocity.y = 0.0f;

                        const float normalSpeed = glm::dot(relativeVelocity, normal);

                        if (normalSpeed > 0.0f)
                        {
                            continue;
                        }

                        const float inverseMassA = 1.0f / a.ball->massKg;
                        const float inverseMassB = 1.0f / b.ball->massKg;

                        const float impulseMagnitude =
                            -(1.0f + restitution) * normalSpeed / (inverseMassA + inverseMassB);

                        const glm::vec3 impulse = impulseMagnitude * normal;

                        a.ball->linearVelocity -= inverseMassA * impulse;
                        b.ball->linearVelocity += inverseMassB * impulse;
                    }
                }
            }

            void updateVisualRolling(std::vector<BallRef>& balls, float dt)
            {
                for (BallRef& ref : balls)
                {
                    if (ref.ball->pocketed)
                    {
                        continue;
                    }

                    glm::vec3 planarVelocity = ref.ball->linearVelocity;
                    planarVelocity.y = 0.0f;

                    const float speed = glm::length(planarVelocity);
                    if (speed < 1.0e-5f)
                    {
                        continue;
                    }

                    const glm::vec3 direction = planarVelocity / speed;
                    const glm::vec3 rollAxis = glm::normalize(
                        glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f))
                    );

                    const float angularSpeed = speed / ref.ball->radius;
                    const float deltaAngle = angularSpeed * dt;

                    const glm::quat deltaRotation = glm::angleAxis(deltaAngle, rollAxis);
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

            applyRollingFriction(balls, tableBounds, dt);
            updateVisualRolling(balls, dt);
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

                    if (glm::length(planarVelocity) > 0.0005f)
                    {
                        moving = true;
                    }
                }
            );

            return moving;
        }
    }
}