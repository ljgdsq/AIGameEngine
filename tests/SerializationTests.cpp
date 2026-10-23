#include "TestFramework.h"

#include "Aster/Scene/SceneSerializer.h"

#include <filesystem>

using namespace Aster;

ASTER_TEST(Serialization, SceneRoundTrip)
{
    Scene scene("RoundTrip");
    scene.GetSettings().Exposure = 1.75f;
    scene.GetSettings().EnableSsao = false;
    scene.GetSettings().EnvironmentPath = "assets/environments/studio.hdr";

    const Entity parent = scene.CreateEntity("Parent");
    scene.GetRegistry().Get<TransformComponent>(parent).Local.Translation = glm::vec3(1.0f, 2.0f, 3.0f);
    const Entity child = scene.CreateEntity("Child");
    scene.SetParent(child, parent);
    CameraComponent camera;
    camera.FieldOfView = 55.0f;
    scene.GetRegistry().Emplace<CameraComponent>(child, camera);
    DirectionalLightComponent light;
    light.Intensity = 4.0f;
    scene.GetRegistry().Emplace<DirectionalLightComponent>(parent, light);
    RigidBodyComponent body;
    body.Type = BodyType::Kinematic;
    scene.GetRegistry().Emplace<RigidBodyComponent>(child, body);
    BoxColliderComponent box;
    box.HalfExtents = glm::vec3(0.25f, 0.5f, 0.75f);
    scene.GetRegistry().Emplace<BoxColliderComponent>(child, box);

    const std::string yaml = SceneSerializer::Serialize(scene);
    ASTER_CHECK(!yaml.empty());

    Scene loaded("Loaded");
    std::string error;
    ASTER_CHECK(SceneSerializer::LoadFromString(loaded, yaml, nullptr, &error));
    ASTER_CHECK_EQ(loaded.GetSettings().Name, std::string("RoundTrip"));
    ASTER_CHECK_NEAR(loaded.GetSettings().Exposure, 1.75f, 1e-5);
    ASTER_CHECK(!loaded.GetSettings().EnableSsao);
    ASTER_CHECK_EQ(loaded.GetSettings().EnvironmentPath, std::string("assets/environments/studio.hdr"));
    ASTER_CHECK_EQ(loaded.GetEntityCount(), 2);

    const Entity loadedParent = loaded.FindByName("Parent");
    const Entity loadedChild = loaded.FindByName("Child");
    ASTER_CHECK(loadedParent != NullEntity);
    ASTER_CHECK(loadedChild != NullEntity);
    ASTER_CHECK_EQ(loaded.GetParent(loadedChild), loadedParent);
    ASTER_CHECK(loaded.GetRegistry().Has<CameraComponent>(loadedChild));
    ASTER_CHECK(loaded.GetRegistry().Has<DirectionalLightComponent>(loadedParent));
    ASTER_CHECK(loaded.GetRegistry().Has<RigidBodyComponent>(loadedChild));
    ASTER_CHECK_EQ(loaded.GetRegistry().Get<RigidBodyComponent>(loadedChild).Type, BodyType::Kinematic);
    ASTER_CHECK_NEAR(loaded.GetRegistry().Get<BoxColliderComponent>(loadedChild).HalfExtents.z, 0.75f, 1e-5);
}

ASTER_TEST(Serialization, InvalidYamlFails)
{
    Scene scene("Test");
    std::string error;
    ASTER_CHECK(!SceneSerializer::LoadFromString(scene, "entities: [", nullptr, &error));
    ASTER_CHECK(!error.empty());
}

ASTER_TEST(Serialization, WrongFormatFails)
{
    Scene scene("Test");
    std::string error;
    ASTER_CHECK(!SceneSerializer::LoadFromString(scene, "format: other\nversion: 1\n", nullptr, &error));
    ASTER_CHECK(!error.empty());
}

ASTER_TEST(Serialization, FileRoundTrip)
{
    Scene scene("File");
    scene.CreateEntity("Entity");
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "aster_scene_roundtrip.aster.yaml";
    std::string error;
    ASTER_CHECK(SceneSerializer::Save(scene, path.string(), &error));

    Scene loaded("Loaded");
    ASTER_CHECK(SceneSerializer::Load(loaded, path.string(), nullptr, &error));
    ASTER_CHECK_EQ(loaded.GetEntityCount(), 1);
    ASTER_CHECK(loaded.FindByName("Entity") != NullEntity);
    std::filesystem::remove(path);
}
