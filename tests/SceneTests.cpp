#include "TestFramework.h"

#include "Aster/Scene/Scene.h"
#include "Aster/Scene/Prefab.h"

#include <filesystem>

using namespace Aster;

ASTER_TEST(Scene, CreateAndFind)
{
    Scene scene("Test");
    const Entity entity = scene.CreateEntity("Player");
    ASTER_CHECK(scene.IsAlive(entity));
    ASTER_CHECK_EQ(scene.FindByName("Player"), entity);
    ASTER_CHECK_EQ(scene.FindByUUID(scene.GetRegistry().Get<IDComponent>(entity).Id), entity);
    ASTER_CHECK_EQ(scene.GetEntityCount(), 1);
}

ASTER_TEST(Scene, HierarchyWorldTransform)
{
    Scene scene("Test");
    const Entity parent = scene.CreateEntity("Parent");
    const Entity child = scene.CreateEntity("Child");
    scene.GetRegistry().Get<TransformComponent>(parent).Local.Translation = glm::vec3(5.0f, 0.0f, 0.0f);
    scene.GetRegistry().Get<TransformComponent>(child).Local.Translation = glm::vec3(0.0f, 2.0f, 0.0f);
    scene.SetParent(child, parent);

    ASTER_CHECK_EQ(scene.GetParent(child), parent);
    ASTER_CHECK_EQ(scene.GetChildren(parent).size(), 1);
    const Transform world = scene.GetWorldTransform(child);
    ASTER_CHECK_NEAR(world.Translation.x, 5.0f, 1e-4);
    ASTER_CHECK_NEAR(world.Translation.y, 2.0f, 1e-4);
}

ASTER_TEST(Scene, PreserveWorldTransformWhenReparenting)
{
    Scene scene("Test");
    const Entity oldParent = scene.CreateEntity("Old");
    const Entity newParent = scene.CreateEntity("New");
    const Entity child = scene.CreateEntity("Child");
    scene.GetRegistry().Get<TransformComponent>(oldParent).Local.Translation = glm::vec3(10.0f, 0.0f, 0.0f);
    scene.GetRegistry().Get<TransformComponent>(newParent).Local.Translation = glm::vec3(0.0f, 10.0f, 0.0f);
    scene.GetRegistry().Get<TransformComponent>(child).Local.Translation = glm::vec3(1.0f, 1.0f, 0.0f);
    scene.SetParent(child, oldParent);

    const Transform before = scene.GetWorldTransform(child);
    scene.SetParent(child, newParent, true);
    const Transform after = scene.GetWorldTransform(child);
    ASTER_CHECK_NEAR(before.Translation.x, after.Translation.x, 1e-3);
    ASTER_CHECK_NEAR(before.Translation.y, after.Translation.y, 1e-3);
}

ASTER_TEST(Scene, DestroyingParentDestroysChildren)
{
    Scene scene("Test");
    const Entity parent = scene.CreateEntity("Parent");
    const Entity child = scene.CreateEntity("Child");
    const Entity grandChild = scene.CreateEntity("GrandChild");
    scene.SetParent(child, parent);
    scene.SetParent(grandChild, child);

    scene.DestroyEntity(parent);
    ASTER_CHECK_EQ(scene.GetEntityCount(), 0);
    ASTER_CHECK(!scene.IsAlive(parent));
    ASTER_CHECK(!scene.IsAlive(child));
    ASTER_CHECK(!scene.IsAlive(grandChild));
}

ASTER_TEST(Scene, RefusesCyclicParenting)
{
    Scene scene("Test");
    const Entity parent = scene.CreateEntity("Parent");
    const Entity child = scene.CreateEntity("Child");
    scene.SetParent(child, parent);
    scene.SetParent(parent, child);
    ASTER_CHECK_EQ(scene.GetParent(parent), NullEntity);
}

ASTER_TEST(Scene, InstantiateSceneCopiesHierarchy)
{
    Scene source("Source");
    const Entity root = source.CreateEntity("Root");
    const Entity child = source.CreateEntity("Child");
    source.SetParent(child, root);
    source.GetRegistry().Get<TransformComponent>(root).Local.Translation = glm::vec3(2.0f, 0.0f, 0.0f);

    Scene target("Target");
    const Entity parent = target.CreateEntity("Parent");
    target.InstantiateScene(source, parent);
    ASTER_CHECK_EQ(target.GetEntityCount(), 3);
    const Entity copiedRoot = target.FindByName("Root");
    ASTER_CHECK(copiedRoot != NullEntity);
    ASTER_CHECK_EQ(target.GetParent(copiedRoot), parent);
    const Entity copiedChild = target.FindByName("Child");
    ASTER_CHECK(copiedChild != NullEntity);
    ASTER_CHECK_EQ(target.GetParent(copiedChild), copiedRoot);
}

ASTER_TEST(Scene, PrefabRoundTrip)
{
    Scene source("Source");
    const Entity root = source.CreateEntity("PrefabRoot");
    const Entity child = source.CreateEntity("PrefabChild");
    source.SetParent(child, root);
    source.GetRegistry().Get<TransformComponent>(root).Local.Translation = glm::vec3(4.0f, 1.0f, 0.0f);

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "aster_prefab.aster.yaml";
    std::string error;
    ASTER_CHECK(Prefab::Save(source, root, path.string(), &error));

    Scene target("Target");
    const Entity parent = target.CreateEntity("Parent");
    ASTER_CHECK(Prefab::Instantiate(path.string(), target, parent, nullptr, &error));
    ASTER_CHECK_EQ(target.GetEntityCount(), 3);
    const Entity copiedRoot = target.FindByName("PrefabRoot");
    ASTER_CHECK(copiedRoot != NullEntity);
    ASTER_CHECK_EQ(target.GetParent(copiedRoot), parent);
    ASTER_CHECK(target.FindByName("PrefabChild") != NullEntity);
    std::filesystem::remove(path);
}

ASTER_TEST(Scene, InstantiateEntityCopiesSubtreeOnly)
{
    Scene source("Source");
    const Entity first = source.CreateEntity("First");
    const Entity child = source.CreateEntity("Child");
    const Entity second = source.CreateEntity("Second");
    source.SetParent(child, first);

    Scene target("Target");
    const Entity copy = target.InstantiateEntity(source, first);
    ASTER_CHECK(copy != NullEntity);
    ASTER_CHECK_EQ(target.GetEntityCount(), 2);
    ASTER_CHECK(target.FindByName("First") != NullEntity);
    ASTER_CHECK(target.FindByName("Child") != NullEntity);
    ASTER_CHECK(target.FindByName("Second") == NullEntity);
}

ASTER_TEST(Scene, PrimaryCameraLookup)
{
    Scene scene("Test");
    scene.CreateEntity("NotCamera");
    const Entity cameraEntity = scene.CreateEntity("Camera");
    CameraComponent camera;
    camera.Primary = true;
    scene.GetRegistry().Emplace<CameraComponent>(cameraEntity, camera);
    ASTER_CHECK_EQ(scene.GetPrimaryCamera(), cameraEntity);
}

ASTER_TEST(Scene, CameraComponentBuildsDepthRange)
{
    Scene scene("Test");
    const Entity entity = scene.CreateEntity("Camera");
    scene.GetRegistry().Get<TransformComponent>(entity).Local.Translation = glm::vec3(0.0f, 0.0f, 5.0f);
    CameraComponent camera;
    camera.NearPlane = 0.1f;
    camera.FarPlane = 100.0f;
    const CameraData data = camera.BuildCameraData(scene.GetWorldMatrix(entity), 16.0f / 9.0f);
    ASTER_CHECK_NEAR(data.Position.z, 5.0f, 1e-4);

    const glm::vec4 nearClip = data.Projection * glm::vec4(0.0f, 0.0f, -0.1f, 1.0f);
    const glm::vec4 farClip = data.Projection * glm::vec4(0.0f, 0.0f, -100.0f, 1.0f);
    ASTER_CHECK_NEAR(nearClip.z / nearClip.w, 0.0f, 1e-3);
    ASTER_CHECK_NEAR(farClip.z / farClip.w, 1.0f, 1e-3);
}
