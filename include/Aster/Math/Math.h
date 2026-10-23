#pragma once

#include "Aster/Core/Base.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace Aster
{
    constexpr float Pi = 3.14159265358979323846f;
    constexpr float TwoPi = Pi * 2.0f;
    constexpr float Epsilon = 1.0e-6f;

    [[nodiscard]] inline float Radians(float degrees) { return glm::radians(degrees); }
    [[nodiscard]] inline float Degrees(float radians) { return glm::degrees(radians); }
    [[nodiscard]] inline float Clamp(float value, float low, float high) { return std::clamp(value, low, high); }
    [[nodiscard]] inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

    struct Transform
    {
        glm::vec3 Translation{ 0.0f };
        glm::vec3 Rotation{ 0.0f }; // Euler angles in degrees, applied as Rz * Ry * Rx.
        glm::vec3 Scale{ 1.0f };

        [[nodiscard]] glm::quat GetRotation() const;
        [[nodiscard]] glm::mat4 GetMatrix() const;

        [[nodiscard]] glm::vec3 TransformPoint(const glm::vec3& point) const;
        [[nodiscard]] glm::vec3 TransformDirection(const glm::vec3& direction) const;
    };

    [[nodiscard]] glm::mat4 ComposeTransform(const glm::vec3& translation, const glm::vec3& rotationDegrees,
        const glm::vec3& scale);
    [[nodiscard]] bool DecomposeTransform(const glm::mat4& matrix, glm::vec3& outTranslation,
        glm::vec3& outRotationDegrees, glm::vec3& outScale);
    [[nodiscard]] bool DecomposeTransform(const glm::mat4& matrix, Transform& outTransform);
    [[nodiscard]] glm::mat4 NormalMatrix(const glm::mat4& model);

    struct Ray
    {
        glm::vec3 Origin{ 0.0f };
        glm::vec3 Direction{ 0.0f, 0.0f, -1.0f };

        [[nodiscard]] glm::vec3 At(float distance) const { return Origin + Direction * distance; }
        [[nodiscard]] Ray Normalized() const;
    };

    struct Aabb
    {
        glm::vec3 Min{ std::numeric_limits<float>::max() };
        glm::vec3 Max{ std::numeric_limits<float>::lowest() };

        [[nodiscard]] bool IsValid() const { return Min.x <= Max.x && Min.y <= Max.y && Min.z <= Max.z; }
        [[nodiscard]] glm::vec3 Center() const { return (Min + Max) * 0.5f; }
        [[nodiscard]] glm::vec3 Extents() const { return (Max - Min) * 0.5f; }
        [[nodiscard]] glm::vec3 Size() const { return Max - Min; }

        void Expand(const glm::vec3& point);
        void Expand(const Aabb& bounds);
        [[nodiscard]] Aabb Transformed(const glm::mat4& matrix) const;
        [[nodiscard]] bool Contains(const glm::vec3& point) const;
        [[nodiscard]] bool Overlaps(const Aabb& other) const;
    };

    // Returns the entry distance if the ray hits the box. When outExit is provided it receives the exit distance.
    [[nodiscard]] std::optional<float> IntersectRayAabb(const Ray& ray, const Aabb& bounds, float maxDistance,
        float* outExit = nullptr);
    [[nodiscard]] std::optional<float> IntersectRaySphere(const Ray& ray, const glm::vec3& center, float radius,
        float maxDistance);
    [[nodiscard]] std::optional<float> IntersectRayPlane(const Ray& ray, const glm::vec3& planePoint,
        const glm::vec3& planeNormal, float maxDistance);

    [[nodiscard]] inline glm::vec3 SafeNormalize(const glm::vec3& value, const glm::vec3& fallback = glm::vec3(0.0f))
    {
        const float lengthSquared = glm::dot(value, value);
        if (lengthSquared <= Epsilon)
        {
            return fallback;
        }
        return value / std::sqrt(lengthSquared);
    }

    [[nodiscard]] inline float SafeAcos(float value)
    {
        return std::acos(std::clamp(value, -1.0f, 1.0f));
    }

    [[nodiscard]] inline bool IsFinite(const glm::vec3& value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }
}
