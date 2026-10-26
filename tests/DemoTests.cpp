#include "TestFramework.h"

#include "Aster/Project/Project.h"
#include "Aster/Rendering/ParticleSystem.h"

#include <filesystem>

using namespace Aster;

ASTER_TEST(Demo, FeatureShowcaseCoversEngineFeatures)
{
    const std::filesystem::path root = std::filesystem::path(ASTER_SOURCE_DIR) / "Samples" / "FeatureShowcase";
    ASTER_CHECK(std::filesystem::exists(root / "project.aster.yaml"));
    ASTER_CHECK(std::filesystem::exists(root / "scenes" / "showcase.aster.yaml"));

    Project project;
    std::string error;
    ASTER_CHECK(project.Open(root, &error));
    Scene& scene = project.GetScene();
    ASTER_CHECK_EQ(scene.GetSettings().Name, std::string("Feature Showcase"));
    ASTER_CHECK(scene.GetSettings().EnableShadows);
    ASTER_CHECK(scene.GetSettings().EnableSsao);
    ASTER_CHECK(scene.GetSettings().EnableBloom);
    ASTER_CHECK(scene.GetSettings().ShowSkybox);
    ASTER_CHECK_EQ(scene.GetSettings().EnvironmentPath, std::string("assets/environments/studio.hdr"));
    ASTER_CHECK(scene.GetPrimaryCamera() != NullEntity);

    size_t directionalLights = 0;
    size_t pointLights = 0;
    size_t spotLights = 0;
    size_t meshes = 0;
    size_t pbrMaterials = 0;
    for (Entity entity : scene.GetEntities())
    {
        Registry& registry = scene.GetRegistry();
        directionalLights += registry.Has<DirectionalLightComponent>(entity) ? 1u : 0u;
        pointLights += registry.Has<PointLightComponent>(entity) ? 1u : 0u;
        spotLights += registry.Has<SpotLightComponent>(entity) ? 1u : 0u;
        if (registry.Has<MeshRendererComponent>(entity))
        {
            ++meshes;
            const MeshRendererComponent& renderer = registry.Get<MeshRendererComponent>(entity);
            for (const Material& material : renderer.GetMaterials())
            {
                if (material.Metallic > 0.9f && material.Roughness < 0.1f)
                {
                    ++pbrMaterials;
                }
            }
        }
    }
    ASTER_CHECK_EQ(directionalLights, static_cast<size_t>(1));
    ASTER_CHECK_EQ(spotLights, static_cast<size_t>(1));
    ASTER_CHECK(pointLights >= static_cast<size_t>(3));
    ASTER_CHECK(meshes >= static_cast<size_t>(7));
    ASTER_CHECK(pbrMaterials >= static_cast<size_t>(1));

    const Entity fire = scene.FindByName("Fire");
    ASTER_CHECK(fire != NullEntity);
    ASTER_CHECK(scene.GetRegistry().Has<ParticleEmitterComponent>(fire));
    const ParticleEmitterComponent& emitter = scene.GetRegistry().Get<ParticleEmitterComponent>(fire);
    ASTER_CHECK(emitter.EmissionRate > 0.0f);
    ASTER_CHECK(emitter.MaxParticles > 0);

    ParticleSystem::Update(scene, 0.5f);
    ASTER_CHECK(!scene.GetRegistry().Get<ParticleEmitterComponent>(fire).Particles.empty());
    const std::vector<ParticleVertex> vertices = ParticleSystem::BuildVertices(scene,
        glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ASTER_CHECK(!vertices.empty());

    const Entity crate = scene.FindByName("Dynamic Crate");
    ASTER_CHECK(crate != NullEntity);
    ASTER_CHECK(scene.GetRegistry().Has<RigidBodyComponent>(crate));
    ASTER_CHECK(scene.GetRegistry().Has<BoxColliderComponent>(crate));

    const Entity camera = scene.FindByName("Main Camera");
    ASTER_CHECK(camera != NullEntity);
    ASTER_CHECK(scene.GetRegistry().Has<AudioListenerComponent>(camera));
    const Entity audio = scene.FindByName("Ambient Audio");
    ASTER_CHECK(audio != NullEntity);
    ASTER_CHECK(scene.GetRegistry().Has<AudioSourceComponent>(audio));

    const Entity animation = scene.FindByName("Animation");
    ASTER_CHECK(animation != NullEntity);
    ASTER_CHECK(scene.GetRegistry().Has<ScriptComponent>(animation));
    ASTER_CHECK(std::filesystem::exists(root / "assets" / "scripts" / "showcase.lua"));
}
