#include "Aster/Rendering/DebugDraw.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace Aster
{
    namespace
    {
        glm::vec3 TransformPoint(const glm::mat4& transform, const glm::vec3& point)
        {
            return glm::vec3(transform * glm::vec4(point, 1.0f));
        }

        void AddLine(std::vector<DebugVertex>& vertices, const glm::vec3& start, const glm::vec3& end,
            const glm::vec4& color)
        {
            vertices.push_back({ start, color });
            vertices.push_back({ end, color });
        }
    }

    void DebugDraw::AppendBox(std::vector<DebugVertex>& vertices, const glm::mat4& transform,
        const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec4& color)
    {
        const glm::vec3 extents = glm::max(halfExtents, glm::vec3(0.0f));
        std::array<glm::vec3, 8> corners{
            glm::vec3(-extents.x, -extents.y, -extents.z),
            glm::vec3(extents.x, -extents.y, -extents.z),
            glm::vec3(extents.x, extents.y, -extents.z),
            glm::vec3(-extents.x, extents.y, -extents.z),
            glm::vec3(-extents.x, -extents.y, extents.z),
            glm::vec3(extents.x, -extents.y, extents.z),
            glm::vec3(extents.x, extents.y, extents.z),
            glm::vec3(-extents.x, extents.y, extents.z)
        };
        for (glm::vec3& corner : corners)
        {
            corner = TransformPoint(transform, corner + center);
        }

        constexpr uint32_t edges[12][2] = {
            { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
            { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
            { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
        };
        for (const auto& edge : edges)
        {
            AddLine(vertices, corners[edge[0]], corners[edge[1]], color);
        }
    }

    void DebugDraw::AppendSphere(std::vector<DebugVertex>& vertices, const glm::mat4& transform,
        const glm::vec3& center, float radius, const glm::vec4& color, uint32_t segments)
    {
        const uint32_t count = std::max(segments, 3u);
        const glm::vec3 worldCenter = TransformPoint(transform, center);
        glm::vec3 axes[3] = {
            glm::normalize(glm::vec3(transform[0])),
            glm::normalize(glm::vec3(transform[1])),
            glm::normalize(glm::vec3(transform[2]))
        };
        for (glm::vec3& axis : axes)
        {
            if (glm::length(axis) < 1e-5f)
            {
                axis = glm::vec3(1.0f, 0.0f, 0.0f);
            }
        }
        const float worldRadius = std::max(radius, 0.0f) * std::max({ glm::length(glm::vec3(transform[0])),
            glm::length(glm::vec3(transform[1])), glm::length(glm::vec3(transform[2])) });

        for (uint32_t circle = 0; circle < 3; ++circle)
        {
            const glm::vec3 axisA = axes[circle];
            const glm::vec3 axisB = axes[(circle + 1) % 3];
            for (uint32_t segment = 0; segment < count; ++segment)
            {
                const float angleA = static_cast<float>(segment) / static_cast<float>(count) * 6.2831853f;
                const float angleB = static_cast<float>(segment + 1) / static_cast<float>(count) * 6.2831853f;
                const glm::vec3 start = worldCenter +
                    (axisA * std::cos(angleA) + axisB * std::sin(angleA)) * worldRadius;
                const glm::vec3 end = worldCenter +
                    (axisA * std::cos(angleB) + axisB * std::sin(angleB)) * worldRadius;
                AddLine(vertices, start, end, color);
            }
        }
    }

    void DebugDraw::AppendSceneColliders(const Scene& scene, std::vector<DebugVertex>& vertices,
        const glm::vec4& color)
    {
        for (Entity entity : scene.GetEntities())
        {
            const Registry& registry = scene.GetRegistry();
            const glm::mat4 world = scene.GetWorldMatrix(entity);
            if (registry.Has<BoxColliderComponent>(entity))
            {
                const BoxColliderComponent& collider = registry.Get<BoxColliderComponent>(entity);
                AppendBox(vertices, world, collider.Center, collider.HalfExtents, color);
            }
            if (registry.Has<SphereColliderComponent>(entity))
            {
                const SphereColliderComponent& collider = registry.Get<SphereColliderComponent>(entity);
                AppendSphere(vertices, world, collider.Center, collider.Radius, color);
            }
        }
    }
}
