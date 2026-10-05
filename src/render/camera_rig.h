#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace BilliardsSaloon
{
    enum class CameraViewMode
    {
        PlayerAim,
        TableOverview,
        ShotFollow,
        FreeLook,
        Broadcast     // the player's aim view, with director cuts while the balls roll
    };

    struct CameraRigState
    {
        CameraViewMode mode {CameraViewMode::Broadcast};
        float freeLookYawRadians {0.0f};
        float freeLookPitchRadians {glm::radians(24.0f)};
        float freeLookDistance {2.35f};
    };

    struct CameraRigInputAxes
    {
        float orbitYaw {0.0f};
        float orbitPitch {0.0f};
        float zoom {0.0f};
    };

    struct CameraRigContext
    {
        glm::vec3 tableCenter {0.0f, 0.0f, 0.0f};
        glm::vec3 cueBallPosition {0.0f, 0.0f, 0.0f};
        glm::vec3 trackedBallPosition {0.0f, 0.0f, 0.0f};
        glm::vec3 trackedBallVelocity {0.0f, 0.0f, 0.0f};
        glm::vec3 aimDirection {0.0f, 0.0f, -1.0f};
        bool cueBallAvailable {false};
        bool trackedBallAvailable {false};
        bool ballsInMotion {false};
    };

    struct CameraPose
    {
        glm::vec3 position {0.0f, 0.0f, 0.0f};
        glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
    };

    [[nodiscard]] const char* cameraViewModeLabel(CameraViewMode mode);

    void applyCameraRigInput(
        CameraRigState& state,
        const CameraRigInputAxes& inputAxes,
        float deltaTimeSeconds);

    // Direct free-look change from mouse motion: orbit angles in radians,
    // zoom in metres (positive moves closer).
    void applyCameraRigDelta(
        CameraRigState& state,
        float yawRadians,
        float pitchRadians,
        float zoomMeters);

    [[nodiscard]] CameraPose desiredCameraPose(
        const CameraRigState& state,
        const CameraRigContext& context);

    // A camera at position looking at target, with world up kept upright.
    [[nodiscard]] CameraPose lookAtPose(const glm::vec3& position, const glm::vec3& target);

    [[nodiscard]] CameraPose blendCameraPose(
        const CameraPose& current,
        const CameraPose& target,
        float positionAlpha,
        float rotationAlpha);
}
