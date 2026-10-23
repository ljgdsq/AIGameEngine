#pragma once

#include "Aster/Scene/Scene.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Aster
{
    struct DebugVertex
    {
        glm::vec3 Position{ 0.0f };
        glm::vec4 Color{ 1.0f };
    };

    // Builds CPU-side line-list geometry for editor and runtime debug overlays. The generated vertices are
    // world-space and can be uploaded directly to a line-list pipeline.
    class DebugDraw
    {
    public:
        static void AppendBox(std::vector<DebugVertex>& vertices, const glm::mat4& transform,
            const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec4& color);
        static void AppendSphere(std::vector<DebugVertex>& vertices, const glm::mat4& transform,
            const glm::vec3& center, float radius, const glm::vec4& color, uint32_t segments = 24);

        // Appends every box and sphere collider in the scene using its world transform. Triggers are drawn
        // in the same color so the overlay remains readable without a second pipeline.
        static void AppendSceneColliders(const Scene& scene, std::vector<DebugVertex>& vertices,
            const glm::vec4& color = glm::vec4(0.12f, 1.0f, 0.25f, 1.0f));
    };
}
