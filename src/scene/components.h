#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "scene/mesh_data.h"

#include <memory>
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
        Sphere,
        Custom    // StaticMeshComponent::custom, built headless and uploaded on first draw
    };

    // Marks scenery that belongs to the hall (rebuilt when the hall changes).
    struct HallTagComponent
    {
        int layout {0};
    };

    struct StaticMeshComponent
    {
        MeshPrimitive primitive {MeshPrimitive::Cube};
        std::shared_ptr<const MeshData> custom;
        bool castsShadow {true};
    };

    enum class MaterialSurfaceType
    {
        Generic,
        Cloth,
        BallResin,
        Wood,
        LampGlass
    };

    struct MaterialComponent
    {
        glm::vec3 albedo {1.0f, 1.0f, 1.0f};
        float specularStrength {0.35f};
        float shininess {32.0f};
        MaterialSurfaceType surfaceType {MaterialSurfaceType::Generic};
        float roughness {0.45f};
        float reflectivity {0.04f};
        float clearcoatStrength {0.0f};
        glm::vec3 emissionColor {0.0f, 0.0f, 0.0f};
        float emissionIntensity {0.0f};
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
