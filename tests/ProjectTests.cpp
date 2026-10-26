#include "TestFramework.h"

#include "Aster/Project/Project.h"
#include "Aster/Physics/PhysicsWorld.h"
#include "Aster/Scripting/ScriptEngine.h"

#include <chrono>
#include <cmath>
#include <filesystem>

using namespace Aster;

namespace
{
    std::filesystem::path MakeTempProjectRoot()
    {
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return std::filesystem::temp_directory_path() / ("aster_project_test_" + std::to_string(stamp));
    }
}

ASTER_TEST(Project, CreatePopulatesDefaultScene)
{
    const std::filesystem::path root = MakeTempProjectRoot();
    std::filesystem::remove_all(root);

    Project project;
    std::string error;
    ASTER_CHECK(project.Create(root, "Test Project", &error));
    ASTER_CHECK(project.IsOpen());
    ASTER_CHECK_EQ(project.GetInfo().Name, "Test Project");
    ASTER_CHECK(std::filesystem::exists(root / "project.aster.yaml"));
    ASTER_CHECK(std::filesystem::exists(root / "assets" / "models" / "cube.obj"));
    ASTER_CHECK(std::filesystem::exists(root / "assets" / "textures" / "checker.png"));
    ASTER_CHECK(std::filesystem::exists(root / "assets" / "textures" / "container2.png"));
    ASTER_CHECK(std::filesystem::exists(root / "assets" / "environments" / "studio.hdr"));
    ASTER_CHECK(std::filesystem::exists(root / "assets" / "sounds" / "ambient.wav"));
    ASTER_CHECK(std::filesystem::exists(root / "assets" / "scripts" / "spin.lua"));
    ASTER_CHECK(std::filesystem::exists(root / "scenes" / "main.aster.yaml"));

    Scene& scene = project.GetScene();
    ASTER_CHECK(scene.FindByName("Sun") != NullEntity);
    ASTER_CHECK(scene.FindByName("Spot Light") != NullEntity);
    ASTER_CHECK(scene.FindByName("Fill Light") != NullEntity);
    ASTER_CHECK(scene.FindByName("Main Camera") != NullEntity);
    ASTER_CHECK(scene.FindByName("Ground") != NullEntity);
    const Entity cube = scene.FindByName("Cube");
    ASTER_CHECK(cube != NullEntity);
    const Entity sphere = scene.FindByName("Sphere");
    ASTER_CHECK(sphere != NullEntity);
    const Entity orb = scene.FindByName("Emissive Orb");
    ASTER_CHECK(orb != NullEntity);
    ASTER_CHECK(scene.GetRegistry().Has<PointLightComponent>(orb));
    ASTER_CHECK(scene.GetRegistry().Has<AudioSourceComponent>(orb));
    ASTER_CHECK(scene.GetRegistry().Has<ParticleEmitterComponent>(orb));
    const AssetRef<Sound> sound = project.GetAssets().LoadSound("assets/sounds/ambient.wav", &error);
    ASTER_CHECK(sound.IsValid());
    ASTER_CHECK(!sound.Asset->EncodedData.empty());
    ASTER_CHECK(scene.GetSettings().EnableBloom);
    ASTER_CHECK(scene.GetSettings().BloomIntensity > 0.0f);
    ASTER_CHECK(scene.GetPrimaryCamera() != NullEntity);
    ASTER_CHECK(scene.GetRegistry().Has<ScriptComponent>(cube));
    const MeshRendererComponent& cubeRenderer = scene.GetRegistry().Get<MeshRendererComponent>(cube);
    ASTER_CHECK(!cubeRenderer.Materials.empty());
    ASTER_CHECK(cubeRenderer.Materials.front().AlbedoMap.IsValid());
    ASTER_CHECK(scene.GetRegistry().Has<RigidBodyComponent>(sphere));
    ASTER_CHECK(scene.GetRegistry().Has<SphereColliderComponent>(sphere));
    ASTER_CHECK(scene.GetRegistry().Has<BoxColliderComponent>(scene.FindByName("Ground")));
    const Entity ground = scene.FindByName("Ground");
    ASTER_CHECK(scene.GetRegistry().Has<MeshRendererComponent>(ground));
    const MeshRendererComponent& groundRenderer = scene.GetRegistry().Get<MeshRendererComponent>(ground);
    ASTER_CHECK(!groundRenderer.Materials.empty());
    ASTER_CHECK(groundRenderer.Materials.front().AlbedoMap.IsValid());
    ASTER_CHECK(!scene.GetSettings().EnvironmentPath.empty());
    const AssetRef<Environment> environment =
        project.GetAssets().LoadEnvironment(scene.GetSettings().EnvironmentPath, &error);
    ASTER_CHECK(environment.IsValid());
    ASTER_CHECK(environment.Asset->PrefilteredSpecular.FaceSize == 64);
    ASTER_CHECK(environment.Asset->BrdfLutSize == 128);

    const Entity cameraEntity = scene.GetPrimaryCamera();
    ASTER_CHECK(cameraEntity != NullEntity);
    const CameraComponent& camera = scene.GetRegistry().Get<CameraComponent>(cameraEntity);
    const CameraData cameraData = camera.BuildCameraData(scene.GetWorldMatrix(cameraEntity), 16.0f / 9.0f);
    const glm::vec4 clip = cameraData.ViewProjection * glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
    ASTER_CHECK(clip.w > 0.0f);
    ASTER_CHECK(std::abs(clip.x / clip.w) < 1.0f);
    ASTER_CHECK(std::abs(clip.y / clip.w) < 1.0f);

    Project reopened;
    ASTER_CHECK(reopened.Open(root, &error));
    const Entity reopenedCube = reopened.GetScene().FindByName("Cube");
    ASTER_CHECK(reopenedCube != NullEntity);
    const MeshRendererComponent& reopenedRenderer =
        reopened.GetScene().GetRegistry().Get<MeshRendererComponent>(reopenedCube);
    ASTER_CHECK(!reopenedRenderer.Materials.empty());
    ASTER_CHECK(reopenedRenderer.Materials.front().AlbedoMap.IsValid());

    std::filesystem::remove_all(root);
}

ASTER_TEST(Project, DefaultSceneRunsScriptsAndPhysics)
{
    const std::filesystem::path root = MakeTempProjectRoot();
    std::filesystem::remove_all(root);

    Project project;
    std::string error;
    ASTER_CHECK(project.Create(root, "Runtime", &error));
    Scene& scene = project.GetScene();
    const Entity cube = scene.FindByName("Cube");
    const Entity sphere = scene.FindByName("Sphere");
    ASTER_CHECK(cube != NullEntity);
    ASTER_CHECK(sphere != NullEntity);

    ScriptEngine scripts;
    ASTER_CHECK(scripts.Initialize(scene, &error));
    ScriptHandle handle;
    ASTER_CHECK(scripts.LoadScript(project.Resolve("assets/scripts/spin.lua").string(), handle, &error));
    ASTER_CHECK(scripts.Start(handle, cube, &error));
    const float startRotation = scene.GetRegistry().Get<TransformComponent>(cube).Local.Rotation.y;
    ASTER_CHECK(scripts.Update(handle, cube, 1.0f, &error));
    ASTER_CHECK_NEAR(scene.GetRegistry().Get<TransformComponent>(cube).Local.Rotation.y, startRotation + 45.0f,
        1.0e-4f);
    scripts.UnloadScript(handle);
    scripts.Shutdown();

    PhysicsWorld physics;
    physics.Rebuild(scene);
    ASTER_CHECK_EQ(physics.GetBodyCount(), 2u);
    const float startHeight = scene.GetRegistry().Get<TransformComponent>(sphere).Local.Translation.y;
    for (int step = 0; step < 120; ++step)
    {
        physics.Step(1.0f / 60.0f);
        physics.SyncToScene(scene);
    }
    const float endHeight = scene.GetRegistry().Get<TransformComponent>(sphere).Local.Translation.y;
    ASTER_CHECK(endHeight < startHeight);
    // Unit sphere (radius 0.5) rests with its center one radius above the ground plane.
    ASTER_CHECK_NEAR(endHeight, 0.5f, 0.03f);

    std::filesystem::remove_all(root);
}

ASTER_TEST(Project, SaveAndReopenRoundTrip)
{
    const std::filesystem::path root = MakeTempProjectRoot();
    std::filesystem::remove_all(root);

    Project project;
    std::string error;
    ASTER_CHECK(project.Create(root, "RoundTrip", &error));
    const Entity cube = project.GetScene().FindByName("Cube");
    ASTER_CHECK(cube != NullEntity);
    project.GetScene().GetRegistry().Get<NameComponent>(cube).Name = "RenamedCube";
    ASTER_CHECK(project.SaveScene(&error));

    Project reopened;
    ASTER_CHECK(reopened.Open(root, &error));
    ASTER_CHECK(reopened.GetScene().FindByName("RenamedCube") != NullEntity);
    ASTER_CHECK(reopened.GetScene().FindByName("Cube") == NullEntity);

    std::filesystem::remove_all(root);
}

ASTER_TEST(Project, OpenRejectsNonProjectDirectory)
{
    const std::filesystem::path root = MakeTempProjectRoot();
    std::filesystem::create_directories(root);
    Project project;
    std::string error;
    ASTER_CHECK(!project.Open(root, &error));
    ASTER_CHECK(!error.empty());
    std::filesystem::remove_all(root);
}

ASTER_TEST(Project, SaveSceneAsCreatesNestedDirectory)
{
    const std::filesystem::path root = MakeTempProjectRoot();
    std::filesystem::remove_all(root);
    Project project;
    std::string error;
    ASTER_CHECK(project.Create(root, "Scenes", &error));
    ASTER_CHECK(project.SaveSceneAs("scenes/levels/level_one.aster.yaml", &error));
    ASTER_CHECK(std::filesystem::exists(root / "scenes" / "levels" / "level_one.aster.yaml"));
    ASTER_CHECK_EQ(project.GetCurrentScenePath(), "scenes/levels/level_one.aster.yaml");
    std::filesystem::remove_all(root);
}
