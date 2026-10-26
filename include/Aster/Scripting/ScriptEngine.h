#pragma once

#include "Aster/ECS/Entity.h"

#include <functional>
#include <string>
#include <string_view>

struct lua_State;

namespace Aster
{
    class AssetManager;
    class PhysicsWorld;
    class Scene;

    // Identifies a script chunk that returned a table. The handle does not own the Lua registry reference;
    // ScriptEngine::UnloadScript releases it.
    struct ScriptHandle
    {
        int Reference = -1;
        std::string Path;

        [[nodiscard]] bool IsValid() const { return Reference >= 0; }
    };

    // Small Lua runtime that exposes the scene to scripts. Script files must return a table containing
    // optional OnStart(entity), OnUpdate(entity, deltaTime) and OnDestroy(entity) functions.
    class ScriptEngine
    {
    public:
        ScriptEngine();
        ~ScriptEngine();

        ScriptEngine(const ScriptEngine&) = delete;
        ScriptEngine& operator=(const ScriptEngine&) = delete;

        [[nodiscard]] bool Initialize(Scene& scene, std::string* error = nullptr);
        void SetAssetManager(AssetManager* assets);
        void SetPhysics(PhysicsWorld* physics);
        void SetKeyDownCallback(std::function<bool(std::string_view)> callback);
        [[nodiscard]] bool IsKeyDown(std::string_view key) const;
        void Shutdown();
        [[nodiscard]] bool IsInitialized() const { return m_State != nullptr; }

        [[nodiscard]] bool LoadScript(const std::string& path, ScriptHandle& handle,
            std::string* error = nullptr);
        [[nodiscard]] bool LoadSource(const std::string& chunkName, const std::string& source,
            ScriptHandle& handle, std::string* error = nullptr);
        void UnloadScript(ScriptHandle& handle);

        [[nodiscard]] bool Start(const ScriptHandle& handle, Entity entity, std::string* error = nullptr);
        [[nodiscard]] bool Update(const ScriptHandle& handle, Entity entity, float deltaTime,
            std::string* error = nullptr);
        [[nodiscard]] bool Destroy(const ScriptHandle& handle, Entity entity, std::string* error = nullptr);
        [[nodiscard]] bool RunString(const std::string& source, std::string* error = nullptr);

        [[nodiscard]] const std::string& GetLastError() const { return m_LastError; }
        [[nodiscard]] lua_State* GetState() const { return m_State; }
        [[nodiscard]] Scene* GetScene() const { return m_Scene; }

    private:
        [[nodiscard]] bool Call(const ScriptHandle& handle, const char* functionName, Entity entity,
            const float* deltaTime, std::string* error);

        lua_State* m_State = nullptr;
        Scene* m_Scene = nullptr;
        AssetManager* m_Assets = nullptr;
        PhysicsWorld* m_Physics = nullptr;
        std::function<bool(std::string_view)> m_KeyDownCallback;
        std::string m_LastError;
    };
}
