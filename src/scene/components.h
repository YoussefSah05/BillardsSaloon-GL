#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>

namespace BilliardsSaloon
{
    struct NameComponent
    {
        std::string value;
    };

    struct TransformComponent
    {
        glm::vec3 previousPosition {0.0f, 0.0f, 0.0f};
        glm::quat previousRotation {1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 previousScale {1.0f, 1.0f, 1.0f};

        glm::vec3 position {0.0f, 0.0f, 0.0f};
        glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale {1.0f, 1.0f, 1.0f};

        void syncPrevious()
        {
            previousPosition = position;
            previousRotation = rotation;
            previousScale = scale;
        }
    };

    struct SpinComponent
    {
        glm::vec3 axis {0.0f, 1.0f, 0.0f};
        float radiansPerSecond {1.0f};
    };

    enum class MeshPrimitive
    {
        Cube,
        Plane,
        Sphere
    };

    struct StaticMeshComponent
    {
        MeshPrimitive primitive {MeshPrimitive::Cube};
    };

    struct MaterialComponent
    {
        glm::vec3 albedo {1.0f, 1.0f, 1.0f};
        float specularStrength {0.35f};
        float shininess {32.0f};
    };

    enum class BallRuleTag
    {
        Cue,
        Solid,
        Stripe,
        Eight,
        Numbered,
        Red,
        Color
    };

    struct BallComponent
    {
        float radius {0.028575f};
        float massKg {0.17f};

        glm::vec3 linearVelocity {0.0f, 0.0f, 0.0f};
        glm::vec3 angularVelocity {0.0f, 0.0f, 0.0f};

        int number {0};
        BallRuleTag ruleTag {BallRuleTag::Numbered};
        bool pocketed {false};
        bool isCueBall {false};
    };

    struct TableBoundsComponent
    {
        float halfWidth {1.42f};
        float halfDepth {0.71f};

        float railRestitution {0.92f};
        float ballRestitution {0.96f};

        float rollingFrictionCoefficient {0.020f};
        float stopSpeedThreshold {0.02f};

        float cornerPocketRadius {0.090f};
        float sidePocketRadius {0.080f};
    };

    struct CameraTagComponent
    {
    };

    struct InterpolatedTransform
    {
        glm::vec3 position {0.0f, 0.0f, 0.0f};
        glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale {1.0f, 1.0f, 1.0f};
    };

    [[nodiscard]] inline InterpolatedTransform interpolateTransform(
        const TransformComponent& transform,
        float alpha)
    {
        InterpolatedTransform result;
        result.position = glm::mix(transform.previousPosition, transform.position, alpha);
        result.rotation = glm::normalize(glm::slerp(transform.previousRotation, transform.rotation, alpha));
        result.scale = glm::mix(transform.previousScale, transform.scale, alpha);
        return result;
    }

    [[nodiscard]] inline glm::mat4 composeMatrix(
        const glm::vec3& position,
        const glm::quat& rotation,
        const glm::vec3& scale)
    {
        const glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
        const glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
        const glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

        return translation * rotationMatrix * scaleMatrix;
    }

    [[nodiscard]] inline glm::mat4 composeInterpolatedMatrix(
        const TransformComponent& transform,
        float alpha)
    {
        const InterpolatedTransform interpolated = interpolateTransform(transform, alpha);
        return composeMatrix(interpolated.position, interpolated.rotation, interpolated.scale);
    }
}