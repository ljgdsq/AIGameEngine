#include "TestFramework.h"

#include "Aster/Rendering/ParticleSystem.h"
#include "Aster/Scene/SceneSerializer.h"
#include "Aster/Scripting/ScriptEngine.h"

#include <cmath>
#include <limits>

using namespace Aster;

namespace
{
    Entity CreateEmitterScene(Scene& scene, ParticleEmitterComponent& emitter)
    {
        const Entity entity = scene.CreateEntity("Fire");
        scene.GetRegistry().Get<TransformComponent>(entity).Local.Translation = glm::vec3(1.0f, 2.0f, 3.0f);
        scene.GetRegistry().Emplace<ParticleEmitterComponent>(entity, emitter);
        return entity;
    }
}

ASTER_TEST(ParticleSystem, EmitsSimulatesAndExpires)
{
    Scene scene("Particles");
    ParticleEmitterComponent emitter;
    emitter.EmissionRate = 8.0f;
    emitter.Lifetime = 1.0f;
    emitter.StartSize = 0.5f;
    emitter.EndSize = 0.0f;
    emitter.Velocity = glm::vec3(0.0f, 1.0f, 0.0f);
    emitter.VelocitySpread = 0.0f;
    emitter.Gravity = glm::vec3(0.0f, -1.0f, 0.0f);
    emitter.MaxParticles = 8;
    const Entity entity = CreateEmitterScene(scene, emitter);

    ParticleSystem::Update(scene, 0.25f);
    const ParticleEmitterComponent& live = scene.GetRegistry().Get<ParticleEmitterComponent>(entity);
    ASTER_CHECK_EQ(live.Particles.size(), static_cast<size_t>(2));
    for (const ParticleState& particle : live.Particles)
    {
        ASTER_CHECK_NEAR(particle.Position.x, 1.0f, 1e-5);
        ASTER_CHECK_NEAR(particle.Position.y, 2.0f, 1e-5);
        ASTER_CHECK_NEAR(particle.Position.z, 3.0f, 1e-5);
        ASTER_CHECK_NEAR(particle.Age, 0.0f, 1e-5);
        ASTER_CHECK_NEAR(particle.Velocity.y, 1.0f, 1e-5);
    }

    const std::vector<ParticleVertex> vertices = ParticleSystem::BuildVertices(scene,
        glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ASTER_CHECK_EQ(vertices.size(), static_cast<size_t>(12));
    for (const ParticleVertex& vertex : vertices)
    {
        ASTER_CHECK(std::isfinite(vertex.Position.x));
        ASTER_CHECK(std::isfinite(vertex.Position.y));
        ASTER_CHECK(std::isfinite(vertex.Position.z));
        ASTER_CHECK_NEAR(vertex.Color.a, 1.0f, 1e-5);
    }

    const std::vector<ParticleVertex> capped = ParticleSystem::BuildVertices(scene,
        glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 7);
    ASTER_CHECK(capped.size() <= static_cast<size_t>(7));
    ASTER_CHECK_EQ(capped.size(), static_cast<size_t>(6));

    ParticleEmitterComponent& mutableEmitter = scene.GetRegistry().Get<ParticleEmitterComponent>(entity);
    mutableEmitter.Emitting = false;
    ParticleSystem::Update(scene, 2.0f);
    ASTER_CHECK(mutableEmitter.Particles.empty());
}

ASTER_TEST(ParticleSystem, CapsParticlesAndIgnoresInvalidDelta)
{
    Scene scene("Particles");
    ParticleEmitterComponent emitter;
    emitter.EmissionRate = 1000.0f;
    emitter.Lifetime = 5.0f;
    emitter.MaxParticles = 5;
    const Entity entity = CreateEmitterScene(scene, emitter);

    ParticleSystem::Update(scene, 0.0f);
    ParticleSystem::Update(scene, -1.0f);
    ParticleSystem::Update(scene, std::numeric_limits<float>::quiet_NaN());
    ASTER_CHECK(scene.GetRegistry().Get<ParticleEmitterComponent>(entity).Particles.empty());

    ParticleSystem::Update(scene, 1.0f);
    ASTER_CHECK_EQ(scene.GetRegistry().Get<ParticleEmitterComponent>(entity).Particles.size(),
        static_cast<size_t>(5));

    ParticleSystem::Reset(scene);
    const ParticleEmitterComponent& reset = scene.GetRegistry().Get<ParticleEmitterComponent>(entity);
    ASTER_CHECK(reset.Particles.empty());
    ASTER_CHECK_NEAR(reset.SpawnAccumulator, 0.0f, 1e-6);
}

ASTER_TEST(ParticleSystem, SerializesEmitterSettingsWithoutRuntimeState)
{
    Scene scene("Particles");
    ParticleEmitterComponent emitter;
    emitter.EmissionRate = 12.5f;
    emitter.Lifetime = 3.25f;
    emitter.StartSize = 0.4f;
    emitter.EndSize = 0.05f;
    emitter.StartColor = glm::vec4(1.0f, 0.5f, 0.25f, 0.9f);
    emitter.EndColor = glm::vec4(0.1f, 0.2f, 0.3f, 0.0f);
    emitter.Velocity = glm::vec3(0.0f, 4.0f, 0.0f);
    emitter.VelocitySpread = 1.5f;
    emitter.Gravity = glm::vec3(0.0f, -3.0f, 0.0f);
    emitter.Offset = glm::vec3(0.0f, 0.5f, 0.0f);
    emitter.MaxParticles = 128;
    const Entity entity = CreateEmitterScene(scene, emitter);
    ParticleSystem::Update(scene, 0.5f);
    ASTER_CHECK(!scene.GetRegistry().Get<ParticleEmitterComponent>(entity).Particles.empty());

    const std::string yaml = SceneSerializer::Serialize(scene);
    Scene loaded("Loaded");
    std::string error;
    ASTER_CHECK(SceneSerializer::LoadFromString(loaded, yaml, nullptr, &error));
    const Entity loadedEntity = loaded.FindByName("Fire");
    ASTER_CHECK(loadedEntity != NullEntity);
    ASTER_CHECK(loaded.GetRegistry().Has<ParticleEmitterComponent>(loadedEntity));
    const ParticleEmitterComponent& loadedEmitter =
        loaded.GetRegistry().Get<ParticleEmitterComponent>(loadedEntity);
    ASTER_CHECK_NEAR(loadedEmitter.EmissionRate, 12.5f, 1e-5);
    ASTER_CHECK_NEAR(loadedEmitter.Lifetime, 3.25f, 1e-5);
    ASTER_CHECK_NEAR(loadedEmitter.StartSize, 0.4f, 1e-5);
    ASTER_CHECK_NEAR(loadedEmitter.EndSize, 0.05f, 1e-5);
    ASTER_CHECK_NEAR(loadedEmitter.StartColor.a, 0.9f, 1e-5);
    ASTER_CHECK_NEAR(loadedEmitter.Velocity.y, 4.0f, 1e-5);
    ASTER_CHECK_NEAR(loadedEmitter.VelocitySpread, 1.5f, 1e-5);
    ASTER_CHECK_NEAR(loadedEmitter.Gravity.y, -3.0f, 1e-5);
    ASTER_CHECK_EQ(loadedEmitter.MaxParticles, static_cast<uint32_t>(128));
    ASTER_CHECK(loadedEmitter.Particles.empty());
}

ASTER_TEST(ParticleSystem, SceneInstantiationCopiesEmitter)
{
    Scene source("Source");
    const Entity torch = source.CreateEntity("Torch");
    ParticleEmitterComponent emitter;
    emitter.EmissionRate = 12.0f;
    emitter.Lifetime = 0.75f;
    emitter.StartColor = glm::vec4(0.2f, 0.4f, 1.0f, 1.0f);
    emitter.MaxParticles = 64;
    source.GetRegistry().Emplace<ParticleEmitterComponent>(torch, emitter);

    Scene target("Target");
    target.InstantiateScene(source);
    const Entity copy = target.FindByName("Torch");
    ASTER_CHECK(copy != NullEntity);
    ASTER_CHECK(target.GetRegistry().Has<ParticleEmitterComponent>(copy));
    const ParticleEmitterComponent& copied = target.GetRegistry().Get<ParticleEmitterComponent>(copy);
    ASTER_CHECK_NEAR(copied.EmissionRate, 12.0f, 1e-5);
    ASTER_CHECK_NEAR(copied.Lifetime, 0.75f, 1e-5);
    ASTER_CHECK_EQ(copied.MaxParticles, static_cast<uint32_t>(64));
    ASTER_CHECK(copied.Particles.empty());
}

ASTER_TEST(ParticleSystem, LuaAuthoringCreatesEmitter)
{
    Scene scene("Particles");
    ScriptEngine scripts;
    std::string error;
    ASTER_CHECK(scripts.Initialize(scene, &error));
    ASTER_CHECK(scripts.RunString(
        "local smoke = scene.CreateEntity('Smoke')\n"
        "smoke:AddParticles(25, 1.25)",
        &error));
    const Entity smoke = scene.FindByName("Smoke");
    ASTER_CHECK(smoke != NullEntity);
    ASTER_CHECK(scene.GetRegistry().Has<ParticleEmitterComponent>(smoke));
    const ParticleEmitterComponent& emitter = scene.GetRegistry().Get<ParticleEmitterComponent>(smoke);
    ASTER_CHECK_NEAR(emitter.EmissionRate, 25.0f, 1e-5);
    ASTER_CHECK_NEAR(emitter.Lifetime, 1.25f, 1e-5);

    ASTER_CHECK(!scripts.RunString("scene.Find('Smoke'):AddParticles()", &error));
    ASTER_CHECK(error.find("already has a particle emitter") != std::string::npos);
    scripts.Shutdown();
}
