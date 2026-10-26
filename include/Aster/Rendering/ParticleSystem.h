#pragma once

#include "Aster/Scene/Scene.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <vector>

namespace Aster
{
    struct ParticleVertex
    {
        glm::vec3 Position{ 0.0f };
        glm::vec4 Color{ 1.0f };
        glm::vec2 UV{ 0.0f };
    };

    // Owns the CPU simulation for every ParticleEmitterComponent in a scene and converts the live
    // particles into camera-facing billboard triangles for the renderer.
    class ParticleSystem
    {
    public:
        static constexpr size_t DefaultMaxVertices = 32768;

        static void Update(Scene& scene, float deltaTime);
        static void Reset(Scene& scene);
        [[nodiscard]] static std::vector<ParticleVertex> BuildVertices(const Scene& scene,
            const glm::vec3& cameraRight, const glm::vec3& cameraUp,
            size_t maxVertices = DefaultMaxVertices);
    };
}
