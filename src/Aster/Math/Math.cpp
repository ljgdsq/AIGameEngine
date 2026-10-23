#include "Aster/Math/Math.h"

namespace Aster
{
    glm::quat Transform::GetRotation() const
    {
        const glm::quat rotateX = glm::angleAxis(Radians(Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::quat rotateY = glm::angleAxis(Radians(Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::quat rotateZ = glm::angleAxis(Radians(Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        return glm::normalize(rotateZ * rotateY * rotateX);
    }

    glm::mat4 Transform::GetMatrix() const
    {
        return ComposeTransform(Translation, Rotation, Scale);
    }

    glm::vec3 Transform::TransformPoint(const glm::vec3& point) const
    {
        return glm::vec3(GetMatrix() * glm::vec4(point, 1.0f));
    }

    glm::vec3 Transform::TransformDirection(const glm::vec3& direction) const
    {
        return glm::mat3(GetMatrix()) * direction;
    }

    glm::mat4 ComposeTransform(const glm::vec3& translation, const glm::vec3& rotationDegrees,
        const glm::vec3& scale)
    {
        const glm::quat rotateX = glm::angleAxis(Radians(rotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::quat rotateY = glm::angleAxis(Radians(rotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::quat rotateZ = glm::angleAxis(Radians(rotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
        const glm::quat rotation = glm::normalize(rotateZ * rotateY * rotateX);

        glm::mat4 result = glm::translate(glm::mat4(1.0f), translation);
        result *= glm::mat4_cast(rotation);
        result = glm::scale(result, scale);
        return result;
    }

    bool DecomposeTransform(const glm::mat4& matrix, glm::vec3& outTranslation, glm::vec3& outRotationDegrees,
        glm::vec3& outScale)
    {
        outTranslation = glm::vec3(matrix[3]);

        glm::vec3 column0(matrix[0]);
        glm::vec3 column1(matrix[1]);
        glm::vec3 column2(matrix[2]);

        float scaleX = glm::length(column0);
        float scaleY = glm::length(column1);
        float scaleZ = glm::length(column2);
        if (!std::isfinite(scaleX) || !std::isfinite(scaleY) || !std::isfinite(scaleZ))
        {
            return false;
        }

        // A negative determinant means an odd number of axes are mirrored. Keep it on X.
        if (glm::determinant(glm::mat3(matrix)) < 0.0f)
        {
            scaleX = -scaleX;
        }

        if (scaleX <= Epsilon || scaleY <= Epsilon || scaleZ <= Epsilon)
        {
            return false;
        }

        glm::mat3 rotation(column0 / scaleX, column1 / scaleY, column2 / scaleZ);

        // Orthonormalize to tolerate small floating point drift.
        const glm::vec3 right = SafeNormalize(rotation[0], glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::vec3 up = SafeNormalize(glm::cross(rotation[2], right), glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::vec3 forward = glm::cross(right, up);
        rotation = glm::mat3(right, up, forward);

        // R = Rz * Ry * Rx, so the matrix elements are:
        // R20 = -sin(pitch), R21 = sin(roll)cos(pitch), R22 = cos(roll)cos(pitch),
        // R10 = cos(pitch)sin(yaw), R00 = cos(pitch)cos(yaw).
        // glm is column-major: element (row, column) is matrix[column][row].
        const float sinPitch = std::clamp(-rotation[0][2], -1.0f, 1.0f);
        const float pitch = std::asin(sinPitch);
        float roll = 0.0f;
        float yaw = 0.0f;
        if (std::abs(sinPitch) < 0.99999f)
        {
            roll = std::atan2(rotation[1][2], rotation[2][2]);
            yaw = std::atan2(rotation[0][1], rotation[0][0]);
        }
        else
        {
            // Gimbal lock: fold the rotation into yaw.
            yaw = std::atan2(-rotation[2][1], rotation[1][1]);
        }

        outRotationDegrees = glm::vec3(Degrees(roll), Degrees(pitch), Degrees(yaw));
        outScale = glm::vec3(scaleX, scaleY, scaleZ);
        return true;
    }

    bool DecomposeTransform(const glm::mat4& matrix, Transform& outTransform)
    {
        return DecomposeTransform(matrix, outTransform.Translation, outTransform.Rotation, outTransform.Scale);
    }

    glm::mat4 NormalMatrix(const glm::mat4& model)
    {
        return glm::transpose(glm::inverse(model));
    }

    Ray Ray::Normalized() const
    {
        Ray result = *this;
        result.Direction = SafeNormalize(Direction, glm::vec3(0.0f, 0.0f, -1.0f));
        return result;
    }

    void Aabb::Expand(const glm::vec3& point)
    {
        Min = glm::min(Min, point);
        Max = glm::max(Max, point);
    }

    void Aabb::Expand(const Aabb& bounds)
    {
        if (!bounds.IsValid())
        {
            return;
        }
        Expand(bounds.Min);
        Expand(bounds.Max);
    }

    Aabb Aabb::Transformed(const glm::mat4& matrix) const
    {
        Aabb result;
        if (!IsValid())
        {
            return result;
        }

        for (int corner = 0; corner < 8; ++corner)
        {
            const glm::vec3 point(
                (corner & 1) != 0 ? Max.x : Min.x,
                (corner & 2) != 0 ? Max.y : Min.y,
                (corner & 4) != 0 ? Max.z : Min.z);
            result.Expand(glm::vec3(matrix * glm::vec4(point, 1.0f)));
        }
        return result;
    }

    bool Aabb::Contains(const glm::vec3& point) const
    {
        return point.x >= Min.x && point.x <= Max.x &&
               point.y >= Min.y && point.y <= Max.y &&
               point.z >= Min.z && point.z <= Max.z;
    }

    bool Aabb::Overlaps(const Aabb& other) const
    {
        return Min.x <= other.Max.x && Max.x >= other.Min.x &&
               Min.y <= other.Max.y && Max.y >= other.Min.y &&
               Min.z <= other.Max.z && Max.z >= other.Min.z;
    }

    std::optional<float> IntersectRayAabb(const Ray& ray, const Aabb& bounds, float maxDistance, float* outExit)
    {
        if (!bounds.IsValid())
        {
            return std::nullopt;
        }

        float tMin = 0.0f;
        float tMax = maxDistance;
        for (int axis = 0; axis < 3; ++axis)
        {
            const float origin = ray.Origin[axis];
            const float direction = ray.Direction[axis];
            if (std::abs(direction) < Epsilon)
            {
                if (origin < bounds.Min[axis] || origin > bounds.Max[axis])
                {
                    return std::nullopt;
                }
                continue;
            }

            const float inverse = 1.0f / direction;
            float near = (bounds.Min[axis] - origin) * inverse;
            float far = (bounds.Max[axis] - origin) * inverse;
            if (near > far)
            {
                std::swap(near, far);
            }
            tMin = std::max(tMin, near);
            tMax = std::min(tMax, far);
            if (tMin > tMax)
            {
                return std::nullopt;
            }
        }

        if (outExit != nullptr)
        {
            *outExit = tMax;
        }
        return tMin;
    }

    std::optional<float> IntersectRaySphere(const Ray& ray, const glm::vec3& center, float radius, float maxDistance)
    {
        if (radius <= 0.0f)
        {
            return std::nullopt;
        }

        const glm::vec3 offset = ray.Origin - center;
        const float a = glm::dot(ray.Direction, ray.Direction);
        const float b = 2.0f * glm::dot(offset, ray.Direction);
        const float c = glm::dot(offset, offset) - radius * radius;
        const float discriminant = b * b - 4.0f * a * c;
        if (discriminant < 0.0f || a <= Epsilon)
        {
            return std::nullopt;
        }

        const float root = std::sqrt(discriminant);
        const float t0 = (-b - root) / (2.0f * a);
        const float t1 = (-b + root) / (2.0f * a);
        if (t0 >= 0.0f && t0 <= maxDistance)
        {
            return t0;
        }
        if (t1 >= 0.0f && t1 <= maxDistance)
        {
            return t1;
        }
        return std::nullopt;
    }

    std::optional<float> IntersectRayPlane(const Ray& ray, const glm::vec3& planePoint, const glm::vec3& planeNormal,
        float maxDistance)
    {
        const float denominator = glm::dot(planeNormal, ray.Direction);
        if (std::abs(denominator) < Epsilon)
        {
            return std::nullopt;
        }
        const float distance = glm::dot(planePoint - ray.Origin, planeNormal) / denominator;
        if (distance < 0.0f || distance > maxDistance)
        {
            return std::nullopt;
        }
        return distance;
    }
}
