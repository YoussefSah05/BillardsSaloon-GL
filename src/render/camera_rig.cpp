#include "render/camera_rig.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace BilliardsSaloon
{
    namespace
    {
        constexpr float FREE_LOOK_YAW_SPEED = glm::radians(85.0f);
        constexpr float FREE_LOOK_PITCH_SPEED = glm::radians(65.0f);
        constexpr float FREE_LOOK_ZOOM_SPEED = 1.6f;

        [[nodiscard]] glm::vec3 safeNormalize(
            const glm::vec3& vector,
            const glm::vec3& fallback)
        {
            const float lengthSquared = glm::dot(vector, vector);
            if (lengthSquared <= 1.0e-8f)
            {
                return fallback;
            }

            return vector / std::sqrt(lengthSquared);
        }

        [[nodiscard]] CameraPose makeLookAtPose(
            const glm::vec3& position,
            const glm::vec3& target,
            const glm::vec3& nominalUp)
        {
            const glm::vec3 forward =
                safeNormalize(target - position, glm::vec3(0.0f, 0.0f, -1.0f));

            glm::vec3 referenceUp = nominalUp;
            if (std::abs(glm::dot(forward, referenceUp)) > 0.98f)
            {
                referenceUp = glm::vec3(0.0f, 0.0f, 1.0f);
            }

            const glm::vec3 right =
                safeNormalize(glm::cross(forward, referenceUp), glm::vec3(1.0f, 0.0f, 0.0f));
            const glm::vec3 up =
                safeNormalize(glm::cross(right, forward), glm::vec3(0.0f, 1.0f, 0.0f));

            const glm::mat3 rotationMatrix(right, up, -forward);

            return CameraPose{
                .position = position,
                .rotation = glm::normalize(glm::quat_cast(rotationMatrix))
            };
        }

        [[nodiscard]] glm::vec3 orbitDirection(float yawRadians, float pitchRadians)
        {
            const float cosPitch = std::cos(pitchRadians);
            return glm::vec3(
                std::sin(yawRadians) * cosPitch,
                std::sin(pitchRadians),
                std::cos(yawRadians) * cosPitch
            );
        }
    }

    const char* cameraViewModeLabel(CameraViewMode mode)
    {
        switch (mode)
        {
            case CameraViewMode::PlayerAim:
                return "Aim";

            case CameraViewMode::TableOverview:
                return "Overview";

            case CameraViewMode::ShotFollow:
                return "Follow";

            case CameraViewMode::FreeLook:
                return "Free";

            case CameraViewMode::Broadcast:
                return "Broadcast";
        }

        return "Unknown";
    }

    void applyCameraRigInput(
        CameraRigState& state,
        const CameraRigInputAxes& inputAxes,
        float deltaTimeSeconds)
    {
        applyCameraRigDelta(
            state,
            inputAxes.orbitYaw * FREE_LOOK_YAW_SPEED * deltaTimeSeconds,
            inputAxes.orbitPitch * FREE_LOOK_PITCH_SPEED * deltaTimeSeconds,
            inputAxes.zoom * FREE_LOOK_ZOOM_SPEED * deltaTimeSeconds
        );
    }

    void applyCameraRigDelta(
        CameraRigState& state,
        float yawRadians,
        float pitchRadians,
        float zoomMeters)
    {
        state.freeLookYawRadians += yawRadians;
        state.freeLookPitchRadians += pitchRadians;
        state.freeLookDistance -= zoomMeters;

        state.freeLookPitchRadians = std::clamp(
            state.freeLookPitchRadians,
            glm::radians(12.0f),
            glm::radians(75.0f)
        );
        state.freeLookDistance = std::clamp(state.freeLookDistance, 1.30f, 4.40f);
    }

    CameraPose lookAtPose(const glm::vec3& position, const glm::vec3& target)
    {
        return makeLookAtPose(position, target, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    CameraPose desiredCameraPose(
        const CameraRigState& state,
        const CameraRigContext& context)
    {
        const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
        const glm::vec3 tableCenter = context.tableCenter;

        switch (state.mode)
        {
            case CameraViewMode::PlayerAim:
            case CameraViewMode::Broadcast:
            {
                const glm::vec3 focus =
                    context.cueBallAvailable
                    ? context.cueBallPosition + glm::vec3(0.0f, 0.02f, 0.0f)
                    : tableCenter;

                const glm::vec3 aimDirection =
                    safeNormalize(context.aimDirection, glm::vec3(0.0f, 0.0f, -1.0f));

                const glm::vec3 position =
                    focus -
                    aimDirection * 0.92f +
                    glm::vec3(0.0f, 0.34f, 0.0f);
                const glm::vec3 lookTarget =
                    focus +
                    aimDirection * 0.38f +
                    glm::vec3(0.0f, 0.05f, 0.0f);

                return makeLookAtPose(position, lookTarget, worldUp);
            }

            case CameraViewMode::TableOverview:
            {
                const glm::vec3 position =
                    tableCenter +
                    glm::vec3(0.0f, 1.92f, 1.70f);
                const glm::vec3 lookTarget =
                    tableCenter +
                    glm::vec3(0.0f, 0.06f, -0.08f);

                return makeLookAtPose(position, lookTarget, worldUp);
            }

            case CameraViewMode::ShotFollow:
            {
                const glm::vec3 focus =
                    context.trackedBallAvailable
                    ? context.trackedBallPosition
                    : (context.cueBallAvailable ? context.cueBallPosition : tableCenter);

                const glm::vec3 referenceDirection =
                    context.ballsInMotion
                    ? safeNormalize(
                        glm::vec3(
                            context.trackedBallVelocity.x,
                            0.0f,
                            context.trackedBallVelocity.z
                        ),
                        safeNormalize(context.aimDirection, glm::vec3(0.0f, 0.0f, -1.0f))
                    )
                    : safeNormalize(context.aimDirection, glm::vec3(0.0f, 0.0f, -1.0f));

                const glm::vec3 position =
                    focus -
                    referenceDirection * 0.80f +
                    glm::vec3(0.0f, 0.42f, 0.0f);
                const glm::vec3 lookTarget =
                    focus +
                    referenceDirection * 0.28f +
                    glm::vec3(0.0f, 0.06f, 0.0f);

                return makeLookAtPose(position, lookTarget, worldUp);
            }

            case CameraViewMode::FreeLook:
            {
                const glm::vec3 focus =
                    tableCenter +
                    glm::vec3(0.0f, 0.06f, 0.0f);
                const glm::vec3 offset =
                    orbitDirection(
                        state.freeLookYawRadians,
                        state.freeLookPitchRadians
                    ) * state.freeLookDistance;

                return makeLookAtPose(focus + offset, focus, worldUp);
            }
        }

        return makeLookAtPose(
            tableCenter + glm::vec3(0.0f, 1.92f, 1.70f),
            tableCenter,
            worldUp
        );
    }

    CameraPose blendCameraPose(
        const CameraPose& current,
        const CameraPose& target,
        float positionAlpha,
        float rotationAlpha)
    {
        return CameraPose{
            .position = glm::mix(current.position, target.position, std::clamp(positionAlpha, 0.0f, 1.0f)),
            .rotation = glm::normalize(glm::slerp(
                current.rotation,
                target.rotation,
                std::clamp(rotationAlpha, 0.0f, 1.0f)
            ))
        };
    }
}
