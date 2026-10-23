#include "TestFramework.h"

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
