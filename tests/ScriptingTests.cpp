#include "TestFramework.h"

#include "Aster/Assets/AssetManager.h"
#include "Aster/Physics/PhysicsWorld.h"
#include "Aster/Scene/Components.h"
#include "Aster/Scene/Scene.h"
#include "Aster/Scripting/ScriptEngine.h"

#include <chrono>
#include <filesystem>
#include <fstream>

using namespace Aster;

namespace
{
    std::filesystem::path MakeTempScriptPath(const char* name)
    {
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return std::filesystem::temp_directory_path() /
            ("aster_script_" + std::to_string(stamp) + "_" + name + ".lua");
    }

    void WriteScript(const std::filesystem::path& path, const std::string& source)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << source;
    }
}

ASTER_TEST(Scripting, StartUpdateDestroyLifecycle)
{
    Scene scene("Script Test");
    const Entity entity = scene.CreateEntity("Actor");
    scene.GetRegistry().Get<TransformComponent>(entity).Local.Translation = glm::vec3(1.0f, 2.0f, 3.0f);

    const std::filesystem::path path = MakeTempScriptPath("lifecycle");
    WriteScript(path, R"(
local script = {}
function script.OnStart(entity)
    entity:SetName("Started")
    entity:SetPosition(2, 4, 6)
end
function script.OnUpdate(entity, deltaTime)
    local x, y, z = entity:GetPosition()
    entity:SetPosition(x + deltaTime, y, z)
    entity:SetRotation(0, 90, 0)
end
function script.OnDestroy(entity)
    entity:SetName("Destroyed")
end
return script
)");

    ScriptEngine scripts;
    std::string error;
    ASTER_CHECK(scripts.Initialize(scene, &error));
    ScriptHandle handle;
    ASTER_CHECK(scripts.LoadScript(path.string(), handle, &error));
    ASTER_CHECK(handle.IsValid());
    ASTER_CHECK(scripts.Start(handle, entity, &error));
    ASTER_CHECK_EQ(scene.GetRegistry().Get<NameComponent>(entity).Name, std::string("Started"));
    ASTER_CHECK_NEAR(scene.GetRegistry().Get<TransformComponent>(entity).Local.Translation.x, 2.0f, 1.0e-5f);
    ASTER_CHECK(scripts.Update(handle, entity, 0.5f, &error));
    ASTER_CHECK_NEAR(scene.GetRegistry().Get<TransformComponent>(entity).Local.Translation.x, 2.5f, 1.0e-5f);
    ASTER_CHECK_NEAR(scene.GetRegistry().Get<TransformComponent>(entity).Local.Rotation.y, 90.0f, 1.0e-5f);
    ASTER_CHECK(scripts.Destroy(handle, entity, &error));
    ASTER_CHECK_EQ(scene.GetRegistry().Get<NameComponent>(entity).Name, std::string("Destroyed"));

    scripts.UnloadScript(handle);
    scripts.Shutdown();
    std::filesystem::remove(path);
}

ASTER_TEST(Scripting, SceneApiCreatesFindsAndDestroysEntities)
{
    Scene scene("Script Scene");
    const Entity entity = scene.CreateEntity("Controller");

    const std::filesystem::path path = MakeTempScriptPath("scene_api");
    WriteScript(path, R"(
local script = {}
function script.OnStart(entity)
    local spawned = scene.CreateEntity("Spawned")
    spawned:SetPosition(4, 5, 6)
    scene.Find("Spawned"):SetScale(2, 2, 2)
    scene.Log("created " .. scene.EntityCount())
end
function script.OnUpdate(entity, deltaTime)
    local spawned = scene.Find("Spawned")
    if spawned ~= nil then
        spawned:Destroy()
    end
end
return script
)");

    ScriptEngine scripts;
    std::string error;
    ASTER_CHECK(scripts.Initialize(scene, &error));
    ScriptHandle handle;
    ASTER_CHECK(scripts.LoadScript(path.string(), handle, &error));
    ASTER_CHECK(scripts.Start(handle, entity, &error));
    const Entity spawned = scene.FindByName("Spawned");
    ASTER_CHECK(spawned != NullEntity);
    ASTER_CHECK_NEAR(scene.GetRegistry().Get<TransformComponent>(spawned).Local.Scale.x, 2.0f, 1.0e-5f);
    ASTER_CHECK(scripts.Update(handle, entity, 0.016f, &error));
    ASTER_CHECK(scene.FindByName("Spawned") == NullEntity);

    scripts.UnloadScript(handle);
    scripts.Shutdown();
    std::filesystem::remove(path);
}

ASTER_TEST(Scripting, LoadRejectsSyntaxErrors)
{
    Scene scene;
    ScriptEngine scripts;
    std::string error;
    ASTER_CHECK(scripts.Initialize(scene, &error));
    ScriptHandle handle;
    ASTER_CHECK(!scripts.LoadSource("broken", "return {", handle, &error));
    ASTER_CHECK(!error.empty());
    ASTER_CHECK(!handle.IsValid());
    ASTER_CHECK(!scripts.GetLastError().empty());
    scripts.Shutdown();
}

ASTER_TEST(Scripting, LoadRejectsChunksWithoutTable)
{
    Scene scene;
    ScriptEngine scripts;
    std::string error;
    ASTER_CHECK(scripts.Initialize(scene, &error));
    ScriptHandle handle;
    ASTER_CHECK(!scripts.LoadSource("not_a_table", "return 42", handle, &error));
    ASTER_CHECK(!error.empty());
    scripts.Shutdown();
}

ASTER_TEST(Scripting, MissingCallbacksAreAllowed)
{
    Scene scene;
    const Entity entity = scene.CreateEntity("Empty");
    ScriptEngine scripts;
    std::string error;
    ASTER_CHECK(scripts.Initialize(scene, &error));
    ScriptHandle handle;
    ASTER_CHECK(scripts.LoadSource("empty", "return {}", handle, &error));
    ASTER_CHECK(scripts.Start(handle, entity, &error));
    ASTER_CHECK(scripts.Update(handle, entity, 0.1f, &error));
    ASTER_CHECK(scripts.Destroy(handle, entity, &error));
    scripts.UnloadScript(handle);
    scripts.Shutdown();
}

ASTER_TEST(Scripting, InvalidHandleFails)
{
    Scene scene;
    ScriptEngine scripts;
    std::string error;
    ASTER_CHECK(scripts.Initialize(scene, &error));
    ScriptHandle handle;
    ASTER_CHECK(!scripts.Update(handle, NullEntity, 0.1f, &error));
    ASTER_CHECK(!error.empty());
    scripts.Shutdown();
}

ASTER_TEST(Scripting, RunStringReportsErrors)
{
    Scene scene;
    ScriptEngine scripts;
    std::string error;
    ASTER_CHECK(scripts.Initialize(scene, &error));
    ASTER_CHECK(scripts.RunString("scene.Log('hello')", &error));
    ASTER_CHECK(!scripts.RunString("error('boom')", &error));
    ASTER_CHECK(error.find("boom") != std::string::npos);
    scripts.Shutdown();
}

ASTER_TEST(Scripting, AuthoringAddsComponents)
{
    Scene scene;
    ScriptEngine scripts;
    std::string error;
    ASTER_CHECK(scripts.Initialize(scene, &error));
    ASTER_CHECK(scripts.RunString(R"(
local actor = scene.CreateEntity("Authored")
actor:AddCamera(70)
actor:AddLight("point", 1, 0, 0, 7)
actor:AddRigidBody("dynamic", 2)
actor:AddBoxCollider(1, 2, 3)
actor:AddScript("assets/scripts/spin.lua")
)", &error));

    const Entity actor = scene.FindByName("Authored");
    ASTER_CHECK(actor != NullEntity);
    const Registry& registry = scene.GetRegistry();
    ASTER_CHECK(registry.Has<CameraComponent>(actor));
    ASTER_CHECK_NEAR(registry.Get<CameraComponent>(actor).FieldOfView, 70.0f, 1.0e-5f);
    ASTER_CHECK(registry.Has<PointLightComponent>(actor));
    ASTER_CHECK_NEAR(registry.Get<PointLightComponent>(actor).Intensity, 7.0f, 1.0e-5f);
    ASTER_CHECK(registry.Has<RigidBodyComponent>(actor));
    ASTER_CHECK(registry.Get<RigidBodyComponent>(actor).Type == BodyType::Dynamic);
    ASTER_CHECK_NEAR(registry.Get<RigidBodyComponent>(actor).Mass, 2.0f, 1.0e-5f);
    ASTER_CHECK(registry.Has<BoxColliderComponent>(actor));
    ASTER_CHECK_NEAR(registry.Get<BoxColliderComponent>(actor).HalfExtents.y, 2.0f, 1.0e-5f);
    ASTER_CHECK(registry.Has<ScriptComponent>(actor));
    ASTER_CHECK_EQ(registry.Get<ScriptComponent>(actor).Scripts.size(), size_t(1));
    ASTER_CHECK_EQ(registry.Get<ScriptComponent>(actor).Scripts[0].Path,
        std::string("assets/scripts/spin.lua"));
    scripts.Shutdown();
}

ASTER_TEST(Scripting, AuthoringAddsMeshAndColor)
{
    Scene scene;
    ScriptEngine scripts;
    std::string error;
    ASTER_CHECK(scripts.Initialize(scene, &error));

    const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("aster_authoring_" + std::to_string(stamp));
    std::filesystem::create_directories(root);
    {
        std::ofstream obj(root / "triangle.obj", std::ios::binary | std::ios::trunc);
        obj << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    }

    AssetManager assets(root);
    scripts.SetAssetManager(&assets);
    ASTER_CHECK(scripts.RunString(R"(
local cube = scene.CreateEntity("Tri")
cube:AddMesh("triangle.obj")
cube:SetColor(1, 0.5, 0.25, 1)
cube:AddSphereCollider(0.4)
)", &error));

    const Entity cube = scene.FindByName("Tri");
    ASTER_CHECK(cube != NullEntity);
    const Registry& registry = scene.GetRegistry();
    ASTER_CHECK(registry.Has<MeshRendererComponent>(cube));
    const MeshRendererComponent& renderer = registry.Get<MeshRendererComponent>(cube);
    ASTER_CHECK(renderer.Mesh.IsValid());
    ASTER_CHECK(renderer.Materials.size() == 1);
    ASTER_CHECK_NEAR(renderer.Materials[0].BaseColor.r, 1.0f, 1.0e-5f);
    ASTER_CHECK_NEAR(renderer.Materials[0].BaseColor.g, 0.5f, 1.0e-5f);
    ASTER_CHECK(registry.Has<SphereColliderComponent>(cube));
    ASTER_CHECK_NEAR(registry.Get<SphereColliderComponent>(cube).Radius, 0.4f, 1.0e-5f);

    scripts.Shutdown();
    std::filesystem::remove_all(root);
}

ASTER_TEST(Scripting, AuthoringMeshRequiresAssetManager)
{
    Scene scene;
    ScriptEngine scripts;
    std::string error;
    ASTER_CHECK(scripts.Initialize(scene, &error));
    ASTER_CHECK(!scripts.RunString("local e = scene.CreateEntity('X'); e:AddMesh('cube.obj')", &error));
    ASTER_CHECK(error.find("asset manager") != std::string::npos);
    scripts.Shutdown();
}

ASTER_TEST(Scripting, PhysicsAndInputBindings)
{
    Scene scene("Script Physics");
    const Entity ball = scene.CreateEntity("Ball");
    scene.GetRegistry().Get<TransformComponent>(ball).Local.Scale = glm::vec3(0.8f);
    RigidBodyComponent body;
    body.Type = BodyType::Dynamic;
    body.UseGravity = false;
    scene.GetRegistry().Emplace<RigidBodyComponent>(ball, body);
    SphereColliderComponent collider;
    collider.Radius = 0.5f;
    scene.GetRegistry().Emplace<SphereColliderComponent>(ball, collider);

    PhysicsWorld physics;
    physics.Rebuild(scene);

    ScriptEngine scripts;
    std::string error;
    ASTER_CHECK(scripts.Initialize(scene, &error));
    scripts.SetPhysics(&physics);
    bool wDown = true;
    // ScriptEngine normalizes script-facing key names to lower case before invoking the callback.
    scripts.SetKeyDownCallback([&wDown](std::string_view key) { return wDown && key == "w"; });
    ASTER_CHECK(scripts.IsKeyDown("W"));
    ASTER_CHECK(!scripts.IsKeyDown("A"));
    ASTER_CHECK(!scripts.IsKeyDown(""));

    ASTER_CHECK(scripts.RunString(R"(
local ball = scene.Find("Ball")
ball:SetLinearVelocity(1, 2, 3)
assert(Input.IsKeyDown("W"))
assert(not Input.IsKeyDown("A"))
assert(not ball:IsGrounded())
)", &error));
    const glm::vec3 velocity = physics.GetLinearVelocity(ball);
    ASTER_CHECK_NEAR(velocity.x, 1.0f, 1.0e-5f);
    ASTER_CHECK_NEAR(velocity.y, 2.0f, 1.0e-5f);
    ASTER_CHECK_NEAR(velocity.z, 3.0f, 1.0e-5f);

    wDown = false;
    ASTER_CHECK(!scripts.IsKeyDown("W"));
    scripts.Shutdown();
}
