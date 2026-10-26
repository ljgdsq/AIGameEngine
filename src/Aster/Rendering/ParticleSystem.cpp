#include "Aster/Rendering/ParticleSystem.h"

#include "Aster/Scene/Components.h"

#include <algorithm>
#include <cmath>

namespace Aster
{
    namespace
    {
        constexpr uint32_t kMaxSimulatedParticles = 4096;

        float NextFloat(uint32_t& state)
        {
            state = state * 1664525u + 1013904223u;
            return static_cast<float>(state >> 8) * (1.0f / 16777216.0f);
        }

        glm::vec3 RandomDirection(uint32_t& state)
        {
            glm::vec3 direction(NextFloat(state) * 2.0f - 1.0f, NextFloat(state) * 2.0f - 1.0f,
                NextFloat(state) * 2.0f - 1.0f);
            if (glm::dot(direction, direction) < 1e-6f)
            {
                return glm::vec3(0.0f, 1.0f, 0.0f);
            }
            return glm::normalize(direction);
        }
    }

    void ParticleSystem::Update(Scene& scene, float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime <= 0.0f)
        {
            return;
        }

        Registry& registry = scene.GetRegistry();
        for (Entity entity : scene.GetEntities())
        {
            if (!registry.Has<ParticleEmitterComponent>(entity))
            {
                continue;
            }

            ParticleEmitterComponent& emitter = registry.Get<ParticleEmitterComponent>(entity);
            const float lifetime = (std::max)(emitter.Lifetime, 0.01f);
            const size_t maxParticles = (std::min)(static_cast<size_t>(emitter.MaxParticles),
                static_cast<size_t>(kMaxSimulatedParticles));

            for (ParticleState& particle : emitter.Particles)
            {
                particle.Age += deltaTime;
                particle.Velocity += emitter.Gravity * deltaTime;
                particle.Position += particle.Velocity * deltaTime;
            }
            std::erase_if(emitter.Particles, [](const ParticleState& particle)
            {
                return particle.Age >= particle.Lifetime;
            });
            if (emitter.Particles.size() > maxParticles)
            {
                emitter.Particles.erase(emitter.Particles.begin(),
                    emitter.Particles.begin() + static_cast<std::ptrdiff_t>(
                        emitter.Particles.size() - maxParticles));
            }

            if (!emitter.Emitting || emitter.EmissionRate <= 0.0f || maxParticles == 0)
            {
                continue;
            }

            emitter.SpawnAccumulator += emitter.EmissionRate * deltaTime;
            if (!std::isfinite(emitter.SpawnAccumulator) || emitter.SpawnAccumulator < 0.0f)
            {
                emitter.SpawnAccumulator = 0.0f;
            }

            const glm::mat4 world = scene.GetWorldMatrix(entity);
            const glm::mat3 rotation = glm::mat3(world);
            const glm::vec3 emitterPosition = glm::vec3(world * glm::vec4(emitter.Offset, 1.0f));
            const glm::vec3 baseVelocity = rotation * emitter.Velocity;
            while (emitter.SpawnAccumulator >= 1.0f && emitter.Particles.size() < maxParticles)
            {
                ParticleState particle;
                particle.Position = emitterPosition;
                particle.Velocity = baseVelocity + RandomDirection(emitter.RandomState) *
                    (std::max)(emitter.VelocitySpread, 0.0f);
                particle.Lifetime = lifetime;
                particle.StartSize = (std::max)(emitter.StartSize, 0.0f);
                particle.EndSize = (std::max)(emitter.EndSize, 0.0f);
                particle.StartColor = emitter.StartColor;
                particle.EndColor = emitter.EndColor;
                emitter.Particles.push_back(particle);
                emitter.SpawnAccumulator -= 1.0f;
            }

            // Never carry more than one emission step into the next frame; a long stall must not
            // cause an unbounded burst once the simulation resumes.
            emitter.SpawnAccumulator = (std::min)(emitter.SpawnAccumulator, 1.0f);
        }
    }

    void ParticleSystem::Reset(Scene& scene)
    {
        Registry& registry = scene.GetRegistry();
        for (Entity entity : scene.GetEntities())
        {
            if (!registry.Has<ParticleEmitterComponent>(entity))
            {
                continue;
            }
            ParticleEmitterComponent& emitter = registry.Get<ParticleEmitterComponent>(entity);
            emitter.Particles.clear();
            emitter.SpawnAccumulator = 0.0f;
            emitter.RandomState = 0x9E3779B9u;
        }
    }

    std::vector<ParticleVertex> ParticleSystem::BuildVertices(const Scene& scene,
        const glm::vec3& cameraRight, const glm::vec3& cameraUp, size_t maxVertices)
    {
        std::vector<ParticleVertex> vertices;
        glm::vec3 right = cameraRight;
        glm::vec3 up = cameraUp;
        if (glm::dot(right, right) < 1e-6f)
        {
            right = glm::vec3(1.0f, 0.0f, 0.0f);
        }
        else
        {
            right = glm::normalize(right);
        }
        if (glm::dot(up, up) < 1e-6f)
        {
            up = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        else
        {
            up = glm::normalize(up);
        }

        const Registry& registry = scene.GetRegistry();
        for (Entity entity : scene.GetEntities())
        {
            if (!registry.Has<ParticleEmitterComponent>(entity))
            {
                continue;
            }
            const ParticleEmitterComponent& emitter = registry.Get<ParticleEmitterComponent>(entity);
            for (const ParticleState& particle : emitter.Particles)
            {
                if (vertices.size() + 6 > maxVertices)
                {
                    return vertices;
                }

                const float normalizedAge = std::clamp(particle.Age /
                    (std::max)(particle.Lifetime, 0.01f), 0.0f, 1.0f);
                const float size = glm::mix(particle.StartSize, particle.EndSize, normalizedAge);
                const glm::vec4 color = glm::mix(particle.StartColor, particle.EndColor, normalizedAge);
                if (size <= 1e-4f || color.a <= 1e-4f)
                {
                    continue;
                }

                const glm::vec3 horizontal = right * size;
                const glm::vec3 vertical = up * size;
                const glm::vec3 bottomLeft = particle.Position - horizontal - vertical;
                const glm::vec3 bottomRight = particle.Position + horizontal - vertical;
                const glm::vec3 topRight = particle.Position + horizontal + vertical;
                const glm::vec3 topLeft = particle.Position - horizontal + vertical;

                vertices.push_back({ bottomLeft, color, glm::vec2(0.0f, 1.0f) });
                vertices.push_back({ bottomRight, color, glm::vec2(1.0f, 1.0f) });
                vertices.push_back({ topRight, color, glm::vec2(1.0f, 0.0f) });
                vertices.push_back({ bottomLeft, color, glm::vec2(0.0f, 1.0f) });
                vertices.push_back({ topRight, color, glm::vec2(1.0f, 0.0f) });
                vertices.push_back({ topLeft, color, glm::vec2(0.0f, 0.0f) });
            }
        }
        return vertices;
    }
}
