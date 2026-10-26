#include "Aster/Scripting/ScriptEngine.h"

#include "Aster/Assets/AssetManager.h"
#include "Aster/Core/Log.h"
#include "Aster/Network/NetworkLua.h"
#include "Aster/Physics/PhysicsWorld.h"
#include "Aster/Scene/Components.h"
#include "Aster/Scene/Scene.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <utility>

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace Aster
{
    namespace
    {
        constexpr const char* kEntityMetatable = "Aster.Entity";
        constexpr const char* kAssetManagerRegistryKey = "Aster.AssetManager";
        constexpr const char* kPhysicsRegistryKey = "Aster.Physics";
        constexpr const char* kScriptEngineRegistryKey = "Aster.ScriptEngine";

        struct LuaEntity
        {
            Scene* ScenePtr = nullptr;
            Entity Value = NullEntity;
        };

        // Lua reports errors by longjmp'ing to the active pcall. On MSVC, longjmp also runs the
        // destructors of C++ objects in the frames it skips, which is unsafe when a binding has
        // locals such as std::string or AssetRef alive. Instead, bindings throw this exception so
        // normal C++ unwinding destroys their locals, and LuaGuard turns it into a Lua error at the
        // C function boundary, where no non-trivial C++ objects remain.
        struct LuaException
        {
            std::string Message;
        };

        int LuaError(lua_State*, const char* message)
        {
            throw LuaException{ message };
        }

        int LuaErrorf(lua_State*, const char* format, ...)
        {
            char buffer[1024];
            va_list arguments;
            va_start(arguments, format);
            std::vsnprintf(buffer, sizeof(buffer), format, arguments);
            va_end(arguments);
            throw LuaException{ buffer };
        }

        template <int (*Function)(lua_State*)>
        int LuaGuard(lua_State* state)
        {
            try
            {
                return Function(state);
            }
            catch (const LuaException& exception)
            {
                lua_pushlstring(state, exception.Message.data(), exception.Message.size());
            }
            return lua_error(state);
        }

        LuaEntity* CheckEntity(lua_State* state, int index)
        {
            return static_cast<LuaEntity*>(luaL_checkudata(state, index, kEntityMetatable));
        }

        LuaEntity* PushEntity(lua_State* state, Scene* scene, Entity entity)
        {
            auto* value = static_cast<LuaEntity*>(lua_newuserdatauv(state, sizeof(LuaEntity), 0));
            value->ScenePtr = scene;
            value->Value = entity;
            luaL_setmetatable(state, kEntityMetatable);
            return value;
        }

        TransformComponent* TryGetTransform(LuaEntity* entity)
        {
            if (entity == nullptr || entity->ScenePtr == nullptr ||
                !entity->ScenePtr->IsAlive(entity->Value))
            {
                return nullptr;
            }
            return entity->ScenePtr->GetRegistry().TryGet<TransformComponent>(entity->Value);
        }

        int EntityGetName(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            if (entity->ScenePtr == nullptr || !entity->ScenePtr->IsAlive(entity->Value))
            {
                lua_pushnil(state);
                return 1;
            }
            const NameComponent* name = entity->ScenePtr->GetRegistry().TryGet<NameComponent>(entity->Value);
            if (name == nullptr)
            {
                lua_pushnil(state);
                return 1;
            }
            lua_pushlstring(state, name->Name.data(), name->Name.size());
            return 1;
        }

        int EntitySetName(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            const char* name = luaL_checkstring(state, 2);
            if (entity->ScenePtr == nullptr || !entity->ScenePtr->IsAlive(entity->Value))
            {
                return LuaError(state, "cannot rename a dead entity");
            }
            NameComponent* component = entity->ScenePtr->GetRegistry().TryGet<NameComponent>(entity->Value);
            if (component == nullptr)
            {
                return LuaError(state, "entity has no name component");
            }
            component->Name = name;
            return 0;
        }

        int EntityGetPosition(lua_State* state)
        {
            TransformComponent* transform = TryGetTransform(CheckEntity(state, 1));
            if (transform == nullptr)
            {
                return LuaError(state, "entity has no transform");
            }
            lua_pushnumber(state, transform->Local.Translation.x);
            lua_pushnumber(state, transform->Local.Translation.y);
            lua_pushnumber(state, transform->Local.Translation.z);
            return 3;
        }

        int EntitySetPosition(lua_State* state)
        {
            TransformComponent* transform = TryGetTransform(CheckEntity(state, 1));
            if (transform == nullptr)
            {
                return LuaError(state, "entity has no transform");
            }
            transform->Local.Translation.x = static_cast<float>(luaL_checknumber(state, 2));
            transform->Local.Translation.y = static_cast<float>(luaL_checknumber(state, 3));
            transform->Local.Translation.z = static_cast<float>(luaL_checknumber(state, 4));
            return 0;
        }

        int EntityGetRotation(lua_State* state)
        {
            TransformComponent* transform = TryGetTransform(CheckEntity(state, 1));
            if (transform == nullptr)
            {
                return LuaError(state, "entity has no transform");
            }
            lua_pushnumber(state, transform->Local.Rotation.x);
            lua_pushnumber(state, transform->Local.Rotation.y);
            lua_pushnumber(state, transform->Local.Rotation.z);
            return 3;
        }

        int EntitySetRotation(lua_State* state)
        {
            TransformComponent* transform = TryGetTransform(CheckEntity(state, 1));
            if (transform == nullptr)
            {
                return LuaError(state, "entity has no transform");
            }
            transform->Local.Rotation.x = static_cast<float>(luaL_checknumber(state, 2));
            transform->Local.Rotation.y = static_cast<float>(luaL_checknumber(state, 3));
            transform->Local.Rotation.z = static_cast<float>(luaL_checknumber(state, 4));
            return 0;
        }

        int EntityGetScale(lua_State* state)
        {
            TransformComponent* transform = TryGetTransform(CheckEntity(state, 1));
            if (transform == nullptr)
            {
                return LuaError(state, "entity has no transform");
            }
            lua_pushnumber(state, transform->Local.Scale.x);
            lua_pushnumber(state, transform->Local.Scale.y);
            lua_pushnumber(state, transform->Local.Scale.z);
            return 3;
        }

        int EntitySetScale(lua_State* state)
        {
            TransformComponent* transform = TryGetTransform(CheckEntity(state, 1));
            if (transform == nullptr)
            {
                return LuaError(state, "entity has no transform");
            }
            transform->Local.Scale.x = static_cast<float>(luaL_checknumber(state, 2));
            transform->Local.Scale.y = static_cast<float>(luaL_checknumber(state, 3));
            transform->Local.Scale.z = static_cast<float>(luaL_checknumber(state, 4));
            return 0;
        }

        int EntityTranslate(lua_State* state)
        {
            TransformComponent* transform = TryGetTransform(CheckEntity(state, 1));
            if (transform == nullptr)
            {
                return LuaError(state, "entity has no transform");
            }
            transform->Local.Translation.x += static_cast<float>(luaL_checknumber(state, 2));
            transform->Local.Translation.y += static_cast<float>(luaL_checknumber(state, 3));
            transform->Local.Translation.z += static_cast<float>(luaL_checknumber(state, 4));
            return 0;
        }

        int EntityDestroy(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            if (entity->ScenePtr != nullptr && entity->ScenePtr->IsAlive(entity->Value))
            {
                entity->ScenePtr->DestroyEntity(entity->Value);
            }
            entity->Value = NullEntity;
            return 0;
        }

        int EntityIsValid(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            lua_pushboolean(state, entity->ScenePtr != nullptr && entity->ScenePtr->IsAlive(entity->Value));
            return 1;
        }

        AssetManager* GetAssets(lua_State* state)
        {
            lua_getfield(state, LUA_REGISTRYINDEX, kAssetManagerRegistryKey);
            auto* assets = static_cast<AssetManager*>(lua_touserdata(state, -1));
            lua_pop(state, 1);
            return assets;
        }

        PhysicsWorld* GetPhysics(lua_State* state)
        {
            lua_getfield(state, LUA_REGISTRYINDEX, kPhysicsRegistryKey);
            auto* physics = static_cast<PhysicsWorld*>(lua_touserdata(state, -1));
            lua_pop(state, 1);
            return physics;
        }

        ScriptEngine* GetScriptEngine(lua_State* state)
        {
            lua_getfield(state, LUA_REGISTRYINDEX, kScriptEngineRegistryKey);
            auto* engine = static_cast<ScriptEngine*>(lua_touserdata(state, -1));
            lua_pop(state, 1);
            return engine;
        }

        int InputIsKeyDown(lua_State* state)
        {
            const char* key = luaL_checkstring(state, 1);
            ScriptEngine* engine = GetScriptEngine(state);
            lua_pushboolean(state, engine != nullptr && engine->IsKeyDown(key));
            return 1;
        }

        int EntityGetLinearVelocity(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            PhysicsWorld* physics = GetPhysics(state);
            if (entity->ScenePtr == nullptr || !entity->ScenePtr->IsAlive(entity->Value) ||
                physics == nullptr)
            {
                return LuaError(state, "physics is not available for this entity");
            }
            const glm::vec3 velocity = physics->GetLinearVelocity(entity->Value);
            lua_pushnumber(state, velocity.x);
            lua_pushnumber(state, velocity.y);
            lua_pushnumber(state, velocity.z);
            return 3;
        }

        int EntitySetLinearVelocity(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            PhysicsWorld* physics = GetPhysics(state);
            if (entity->ScenePtr == nullptr || !entity->ScenePtr->IsAlive(entity->Value) ||
                physics == nullptr)
            {
                return LuaError(state, "physics is not available for this entity");
            }
            physics->SetLinearVelocity(entity->Value,
                glm::vec3(static_cast<float>(luaL_checknumber(state, 2)),
                    static_cast<float>(luaL_checknumber(state, 3)),
                    static_cast<float>(luaL_checknumber(state, 4))));
            return 0;
        }

        int EntityIsGrounded(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            PhysicsWorld* physics = GetPhysics(state);
            if (entity->ScenePtr == nullptr || !entity->ScenePtr->IsAlive(entity->Value) ||
                physics == nullptr)
            {
                return LuaError(state, "physics is not available for this entity");
            }
            lua_pushboolean(state, physics->IsGrounded(entity->Value));
            return 1;
        }

        int EntityAddMesh(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            if (entity->ScenePtr == nullptr || !entity->ScenePtr->IsAlive(entity->Value))
            {
                return LuaError(state, "cannot add a mesh to a dead entity");
            }
            AssetManager* assets = GetAssets(state);
            if (assets == nullptr)
            {
                return LuaError(state, "no asset manager is available");
            }

            const char* path = luaL_checkstring(state, 2);
            std::string error;
            AssetRef<MeshAsset> mesh = assets->LoadMesh(path, &error);
            if (!mesh.IsValid())
            {
                return LuaErrorf(state, "could not load mesh '%s': %s", path, error.c_str());
            }

            MeshRendererComponent renderer;
            renderer.Mesh = std::move(mesh);
            entity->ScenePtr->GetRegistry().Emplace<MeshRendererComponent>(entity->Value,
                std::move(renderer));
            return 0;
        }

        int EntitySetColor(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            if (entity->ScenePtr == nullptr || !entity->ScenePtr->IsAlive(entity->Value))
            {
                return LuaError(state, "cannot color a dead entity");
            }
            MeshRendererComponent* renderer =
                entity->ScenePtr->GetRegistry().TryGet<MeshRendererComponent>(entity->Value);
            if (renderer == nullptr)
            {
                return LuaError(state, "entity has no mesh renderer");
            }
            if (renderer->Materials.empty())
            {
                if (renderer->Mesh.IsValid())
                {
                    renderer->Materials = renderer->Mesh->Materials;
                }
                if (renderer->Materials.empty())
                {
                    renderer->Materials.emplace_back();
                }
            }

            const glm::vec4 color(static_cast<float>(luaL_optnumber(state, 2, 1.0)),
                static_cast<float>(luaL_optnumber(state, 3, 1.0)),
                static_cast<float>(luaL_optnumber(state, 4, 1.0)),
                static_cast<float>(luaL_optnumber(state, 5, 1.0)));
            for (Material& material : renderer->Materials)
            {
                material.BaseColor = color;
            }
            return 0;
        }

        int EntityAddCamera(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            if (entity->ScenePtr == nullptr || !entity->ScenePtr->IsAlive(entity->Value))
            {
                return LuaError(state, "cannot add a camera to a dead entity");
            }
            CameraComponent camera;
            camera.FieldOfView = static_cast<float>(luaL_optnumber(state, 2, camera.FieldOfView));
            entity->ScenePtr->GetRegistry().Emplace<CameraComponent>(entity->Value, camera);
            return 0;
        }

        int EntityAddLight(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            if (entity->ScenePtr == nullptr || !entity->ScenePtr->IsAlive(entity->Value))
            {
                return LuaError(state, "cannot add a light to a dead entity");
            }
            const std::string kind = luaL_optstring(state, 2, "point");
            const glm::vec3 color(static_cast<float>(luaL_optnumber(state, 3, 1.0)),
                static_cast<float>(luaL_optnumber(state, 4, 1.0)),
                static_cast<float>(luaL_optnumber(state, 5, 1.0)));
            if (kind == "directional")
            {
                DirectionalLightComponent light;
                light.Color = color;
                light.Intensity = static_cast<float>(luaL_optnumber(state, 6, light.Intensity));
                entity->ScenePtr->GetRegistry().Emplace<DirectionalLightComponent>(entity->Value, light);
            }
            else if (kind == "spot")
            {
                SpotLightComponent light;
                light.Color = color;
                light.Intensity = static_cast<float>(luaL_optnumber(state, 6, light.Intensity));
                entity->ScenePtr->GetRegistry().Emplace<SpotLightComponent>(entity->Value, light);
            }
            else if (kind == "point")
            {
                PointLightComponent light;
                light.Color = color;
                light.Intensity = static_cast<float>(luaL_optnumber(state, 6, light.Intensity));
                entity->ScenePtr->GetRegistry().Emplace<PointLightComponent>(entity->Value, light);
            }
            else
            {
                return LuaErrorf(state, "unknown light kind '%s' (use directional, point, or spot)",
                    kind.c_str());
            }
            return 0;
        }

        int EntityAddRigidBody(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            if (entity->ScenePtr == nullptr || !entity->ScenePtr->IsAlive(entity->Value))
            {
                return LuaError(state, "cannot add a rigid body to a dead entity");
            }
            const std::string kind = luaL_optstring(state, 2, "dynamic");
            RigidBodyComponent body;
            if (kind == "static")
            {
                body.Type = BodyType::Static;
            }
            else if (kind == "kinematic")
            {
                body.Type = BodyType::Kinematic;
            }
            else if (kind != "dynamic")
            {
                return LuaErrorf(state, "unknown body kind '%s' (use static, kinematic, or dynamic)",
                    kind.c_str());
            }
            body.Mass = static_cast<float>(luaL_optnumber(state, 3, body.Mass));
            entity->ScenePtr->GetRegistry().Emplace<RigidBodyComponent>(entity->Value, body);
            return 0;
        }

        int EntityAddBoxCollider(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            if (entity->ScenePtr == nullptr || !entity->ScenePtr->IsAlive(entity->Value))
            {
                return LuaError(state, "cannot add a collider to a dead entity");
            }
            BoxColliderComponent collider;
            collider.HalfExtents = glm::vec3(static_cast<float>(luaL_optnumber(state, 2, 0.5)),
                static_cast<float>(luaL_optnumber(state, 3, 0.5)),
                static_cast<float>(luaL_optnumber(state, 4, 0.5)));
            entity->ScenePtr->GetRegistry().Emplace<BoxColliderComponent>(entity->Value, collider);
            return 0;
        }

        int EntityAddSphereCollider(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            if (entity->ScenePtr == nullptr || !entity->ScenePtr->IsAlive(entity->Value))
            {
                return LuaError(state, "cannot add a collider to a dead entity");
            }
            SphereColliderComponent collider;
            collider.Radius = static_cast<float>(luaL_optnumber(state, 2, collider.Radius));
            entity->ScenePtr->GetRegistry().Emplace<SphereColliderComponent>(entity->Value, collider);
            return 0;
        }

        int EntityAddScript(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            if (entity->ScenePtr == nullptr || !entity->ScenePtr->IsAlive(entity->Value))
            {
                return LuaError(state, "cannot add a script to a dead entity");
            }
            const char* path = luaL_checkstring(state, 2);
            ScriptComponent* scripts =
                entity->ScenePtr->GetRegistry().TryGet<ScriptComponent>(entity->Value);
            if (scripts == nullptr)
            {
                scripts = &entity->ScenePtr->GetRegistry().Emplace<ScriptComponent>(entity->Value);
            }
            scripts->Scripts.push_back(ScriptEntry{ path, true });
            return 0;
        }

        int EntityAddParticles(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            if (entity->ScenePtr == nullptr || !entity->ScenePtr->IsAlive(entity->Value))
            {
                return LuaError(state, "cannot add particles to a dead entity");
            }
            Registry& registry = entity->ScenePtr->GetRegistry();
            if (registry.Has<ParticleEmitterComponent>(entity->Value))
            {
                return LuaError(state, "entity already has a particle emitter");
            }
            ParticleEmitterComponent emitter;
            emitter.EmissionRate = (std::max)(0.0f, static_cast<float>(luaL_optnumber(state, 2,
                emitter.EmissionRate)));
            emitter.Lifetime = (std::max)(0.01f, static_cast<float>(luaL_optnumber(state, 3,
                emitter.Lifetime)));
            registry.Emplace<ParticleEmitterComponent>(entity->Value, std::move(emitter));
            return 0;
        }

        int EntityAddAudio(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            if (entity->ScenePtr == nullptr || !entity->ScenePtr->IsAlive(entity->Value))
            {
                return LuaError(state, "cannot add audio to a dead entity");
            }
            AssetManager* assets = GetAssets(state);
            if (assets == nullptr)
            {
                return LuaError(state, "no asset manager is available");
            }
            const char* path = luaL_checkstring(state, 2);
            std::string error;
            AssetRef<Sound> sound = assets->LoadSound(path, &error);
            if (!sound.IsValid())
            {
                return LuaErrorf(state, "could not load sound '%s': %s", path, error.c_str());
            }
            AudioSourceComponent source;
            source.Clip = std::move(sound);
            source.Loop = lua_toboolean(state, 3) != 0;
            entity->ScenePtr->GetRegistry().Emplace<AudioSourceComponent>(entity->Value, source);
            return 0;
        }

        int EntityToString(lua_State* state)
        {
            LuaEntity* entity = CheckEntity(state, 1);
            if (entity->ScenePtr == nullptr || !entity->ScenePtr->IsAlive(entity->Value))
            {
                lua_pushliteral(state, "Entity(dead)");
                return 1;
            }
            const NameComponent* name = entity->ScenePtr->GetRegistry().TryGet<NameComponent>(entity->Value);
            const std::string text = name != nullptr
                ? "Entity(" + name->Name + ")"
                : "Entity(" + entity->Value.ToString() + ")";
            lua_pushlstring(state, text.data(), text.size());
            return 1;
        }

        int Traceback(lua_State* state)
        {
            const char* message = lua_tostring(state, 1);
            luaL_traceback(state, state, message, 1);
            return 1;
        }

        int SceneCreateEntity(lua_State* state)
        {
            Scene* scene = static_cast<Scene*>(lua_touserdata(state, lua_upvalueindex(1)));
            const char* name = luaL_optstring(state, 1, "Entity");
            PushEntity(state, scene, scene->CreateEntity(name));
            return 1;
        }

        int SceneFindEntity(lua_State* state)
        {
            Scene* scene = static_cast<Scene*>(lua_touserdata(state, lua_upvalueindex(1)));
            const char* name = luaL_checkstring(state, 1);
            const Entity entity = scene->FindByName(name);
            if (entity == NullEntity)
            {
                lua_pushnil(state);
                return 1;
            }
            PushEntity(state, scene, entity);
            return 1;
        }

        int SceneGetEntityCount(lua_State* state)
        {
            Scene* scene = static_cast<Scene*>(lua_touserdata(state, lua_upvalueindex(1)));
            lua_pushinteger(state, static_cast<lua_Integer>(scene->GetEntityCount()));
            return 1;
        }

        int SceneDestroyEntity(lua_State* state)
        {
            Scene* scene = static_cast<Scene*>(lua_touserdata(state, lua_upvalueindex(1)));
            LuaEntity* entity = CheckEntity(state, 1);
            if (entity->ScenePtr != scene || !scene->IsAlive(entity->Value))
            {
                return LuaError(state, "entity does not belong to this scene");
            }
            scene->DestroyEntity(entity->Value);
            entity->Value = NullEntity;
            return 0;
        }

        int SceneLog(lua_State* state)
        {
            const char* message = luaL_checkstring(state, 1);
            ASTER_LOG_INFO("Lua", "%s", message);
            return 0;
        }

        void RegisterEntityType(lua_State* state)
        {
            luaL_newmetatable(state, kEntityMetatable);
            lua_pushcfunction(state, LuaGuard<EntityToString>);
            lua_setfield(state, -2, "__tostring");

            lua_newtable(state);
            const luaL_Reg methods[] = {
                { "GetName", LuaGuard<EntityGetName> },
                { "SetName", LuaGuard<EntitySetName> },
                { "GetPosition", LuaGuard<EntityGetPosition> },
                { "SetPosition", LuaGuard<EntitySetPosition> },
                { "GetRotation", LuaGuard<EntityGetRotation> },
                { "SetRotation", LuaGuard<EntitySetRotation> },
                { "GetScale", LuaGuard<EntityGetScale> },
                { "SetScale", LuaGuard<EntitySetScale> },
                { "Translate", LuaGuard<EntityTranslate> },
                { "GetLinearVelocity", LuaGuard<EntityGetLinearVelocity> },
                { "SetLinearVelocity", LuaGuard<EntitySetLinearVelocity> },
                { "IsGrounded", LuaGuard<EntityIsGrounded> },
                { "Destroy", LuaGuard<EntityDestroy> },
                { "IsValid", LuaGuard<EntityIsValid> },
                { "AddMesh", LuaGuard<EntityAddMesh> },
                { "SetColor", LuaGuard<EntitySetColor> },
                { "AddCamera", LuaGuard<EntityAddCamera> },
                { "AddLight", LuaGuard<EntityAddLight> },
                { "AddRigidBody", LuaGuard<EntityAddRigidBody> },
                { "AddBoxCollider", LuaGuard<EntityAddBoxCollider> },
                { "AddSphereCollider", LuaGuard<EntityAddSphereCollider> },
                { "AddScript", LuaGuard<EntityAddScript> },
                { "AddAudio", LuaGuard<EntityAddAudio> },
                { "AddParticles", LuaGuard<EntityAddParticles> },
                { nullptr, nullptr }
            };
            luaL_setfuncs(state, methods, 0);
            lua_setfield(state, -2, "__index");
            lua_pop(state, 1);
        }

        void RegisterInput(lua_State* state)
        {
            lua_newtable(state);
            lua_pushcfunction(state, LuaGuard<InputIsKeyDown>);
            lua_setfield(state, -2, "IsKeyDown");
            lua_setglobal(state, "Input");
        }

        void RegisterScene(lua_State* state, Scene* scene)
        {
            lua_newtable(state);
            lua_pushlightuserdata(state, scene);
            lua_pushcclosure(state, LuaGuard<SceneCreateEntity>, 1);
            lua_setfield(state, -2, "CreateEntity");

            lua_pushlightuserdata(state, scene);
            lua_pushcclosure(state, LuaGuard<SceneFindEntity>, 1);
            lua_setfield(state, -2, "Find");

            lua_pushlightuserdata(state, scene);
            lua_pushcclosure(state, LuaGuard<SceneGetEntityCount>, 1);
            lua_setfield(state, -2, "EntityCount");

            lua_pushlightuserdata(state, scene);
            lua_pushcclosure(state, LuaGuard<SceneDestroyEntity>, 1);
            lua_setfield(state, -2, "Destroy");

            lua_pushcfunction(state, LuaGuard<SceneLog>);
            lua_setfield(state, -2, "Log");
            lua_setglobal(state, "scene");
        }

        bool ReadFile(const std::string& path, std::string& contents)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
            {
                return false;
            }
            std::ostringstream stream;
            stream << file.rdbuf();
            if (!file.good() && !file.eof())
            {
                return false;
            }
            contents = stream.str();
            return true;
        }
    }

    ScriptEngine::ScriptEngine() = default;

    ScriptEngine::~ScriptEngine()
    {
        Shutdown();
    }

    bool ScriptEngine::Initialize(Scene& scene, std::string* error)
    {
        Shutdown();
        m_Scene = &scene;
        m_State = luaL_newstate();
        if (m_State == nullptr)
        {
            m_LastError = "Could not create the Lua state";
            if (error != nullptr)
            {
                *error = m_LastError;
            }
            return false;
        }
        luaL_openlibs(m_State);
        RegisterEntityType(m_State);
        RegisterScene(m_State, m_Scene);
        RegisterInput(m_State);
        RegisterNetworkBindings(m_State);

        // Bind the owning engine and its optional subsystems for the duration of this Lua state.
        lua_pushlightuserdata(m_State, this);
        lua_setfield(m_State, LUA_REGISTRYINDEX, kScriptEngineRegistryKey);
        SetAssetManager(m_Assets);
        SetPhysics(m_Physics);

        m_LastError.clear();
        ASTER_LOG_INFO("Script", "Lua %s initialized", LUA_RELEASE);
        return true;
    }

    void ScriptEngine::SetAssetManager(AssetManager* assets)
    {
        m_Assets = assets;
        if (m_State != nullptr)
        {
            if (m_Assets != nullptr)
            {
                lua_pushlightuserdata(m_State, m_Assets);
            }
            else
            {
                lua_pushnil(m_State);
            }
            lua_setfield(m_State, LUA_REGISTRYINDEX, kAssetManagerRegistryKey);
        }
    }

    void ScriptEngine::SetPhysics(PhysicsWorld* physics)
    {
        m_Physics = physics;
        if (m_State != nullptr)
        {
            if (m_Physics != nullptr)
            {
                lua_pushlightuserdata(m_State, m_Physics);
            }
            else
            {
                lua_pushnil(m_State);
            }
            lua_setfield(m_State, LUA_REGISTRYINDEX, kPhysicsRegistryKey);
        }
    }

    void ScriptEngine::SetKeyDownCallback(std::function<bool(std::string_view)> callback)
    {
        m_KeyDownCallback = std::move(callback);
    }

    bool ScriptEngine::IsKeyDown(std::string_view key) const
    {
        if (!m_KeyDownCallback || key.empty())
        {
            return false;
        }
        std::string normalized(key);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return m_KeyDownCallback(normalized);
    }

    void ScriptEngine::Shutdown()
    {
        if (m_State != nullptr)
        {
            lua_close(m_State);
            m_State = nullptr;
        }
        m_Scene = nullptr;
        m_Assets = nullptr;
        m_Physics = nullptr;
        m_KeyDownCallback = nullptr;
        m_LastError.clear();
    }

    bool ScriptEngine::LoadScript(const std::string& path, ScriptHandle& handle, std::string* error)
    {
        if (m_State == nullptr)
        {
            m_LastError = "The script engine is not initialized";
            if (error != nullptr)
            {
                *error = m_LastError;
            }
            return false;
        }

        std::string source;
        if (!ReadFile(path, source))
        {
            m_LastError = "Could not read script: " + path;
            if (error != nullptr)
            {
                *error = m_LastError;
            }
            ASTER_LOG_ERROR("Script", "%s", m_LastError.c_str());
            return false;
        }
        return LoadSource(path, source, handle, error);
    }

    bool ScriptEngine::LoadSource(const std::string& chunkName, const std::string& source,
        ScriptHandle& handle, std::string* error)
    {
        if (m_State == nullptr)
        {
            m_LastError = "The script engine is not initialized";
            if (error != nullptr)
            {
                *error = m_LastError;
            }
            return false;
        }

        lua_pushcfunction(m_State, Traceback);
        const int errorFunction = lua_gettop(m_State);
        int status = luaL_loadbufferx(m_State, source.data(), source.size(), chunkName.c_str(), "t");
        if (status == LUA_OK)
        {
            status = lua_pcall(m_State, 0, 1, errorFunction);
        }
        if (status != LUA_OK)
        {
            const char* message = lua_tostring(m_State, -1);
            m_LastError = message != nullptr ? message : "Unknown Lua error";
            if (error != nullptr)
            {
                *error = m_LastError;
            }
            ASTER_LOG_ERROR("Script", "%s: %s", chunkName.c_str(), m_LastError.c_str());
            lua_settop(m_State, errorFunction - 1);
            return false;
        }

        if (!lua_istable(m_State, -1))
        {
            m_LastError = "Script must return a table: " + chunkName;
            if (error != nullptr)
            {
                *error = m_LastError;
            }
            ASTER_LOG_ERROR("Script", "%s", m_LastError.c_str());
            lua_settop(m_State, errorFunction - 1);
            return false;
        }

        handle.Reference = luaL_ref(m_State, LUA_REGISTRYINDEX);
        handle.Path = chunkName;
        lua_settop(m_State, errorFunction - 1);
        m_LastError.clear();
        return true;
    }

    void ScriptEngine::UnloadScript(ScriptHandle& handle)
    {
        if (m_State != nullptr && handle.IsValid())
        {
            luaL_unref(m_State, LUA_REGISTRYINDEX, handle.Reference);
        }
        handle.Reference = -1;
        handle.Path.clear();
    }

    bool ScriptEngine::Start(const ScriptHandle& handle, Entity entity, std::string* error)
    {
        return Call(handle, "OnStart", entity, nullptr, error);
    }

    bool ScriptEngine::Update(const ScriptHandle& handle, Entity entity, float deltaTime, std::string* error)
    {
        return Call(handle, "OnUpdate", entity, &deltaTime, error);
    }

    bool ScriptEngine::Destroy(const ScriptHandle& handle, Entity entity, std::string* error)
    {
        return Call(handle, "OnDestroy", entity, nullptr, error);
    }

    bool ScriptEngine::Call(const ScriptHandle& handle, const char* functionName, Entity entity,
        const float* deltaTime, std::string* error)
    {
        if (m_State == nullptr || !handle.IsValid())
        {
            m_LastError = "Invalid script handle";
            if (error != nullptr)
            {
                *error = m_LastError;
            }
            return false;
        }

        lua_pushcfunction(m_State, Traceback);
        const int errorFunction = lua_gettop(m_State);
        lua_rawgeti(m_State, LUA_REGISTRYINDEX, handle.Reference);
        if (!lua_istable(m_State, -1))
        {
            m_LastError = "Script handle does not reference a table";
            if (error != nullptr)
            {
                *error = m_LastError;
            }
            lua_settop(m_State, errorFunction - 1);
            return false;
        }

        lua_getfield(m_State, -1, functionName);
        if (!lua_isfunction(m_State, -1))
        {
            lua_settop(m_State, errorFunction - 1);
            m_LastError.clear();
            return true;
        }

        lua_remove(m_State, -2);
        PushEntity(m_State, m_Scene, entity);
        int argumentCount = 1;
        if (deltaTime != nullptr)
        {
            lua_pushnumber(m_State, *deltaTime);
            argumentCount = 2;
        }

        const int status = lua_pcall(m_State, argumentCount, 0, errorFunction);
        if (status != LUA_OK)
        {
            const char* message = lua_tostring(m_State, -1);
            m_LastError = message != nullptr ? message : "Unknown Lua error";
            if (error != nullptr)
            {
                *error = m_LastError;
            }
            ASTER_LOG_ERROR("Script", "%s: %s", handle.Path.c_str(), m_LastError.c_str());
            lua_settop(m_State, errorFunction - 1);
            return false;
        }

        lua_settop(m_State, errorFunction - 1);
        m_LastError.clear();
        return true;
    }

    bool ScriptEngine::RunString(const std::string& source, std::string* error)
    {
        if (m_State == nullptr)
        {
            m_LastError = "The script engine is not initialized";
            if (error != nullptr)
            {
                *error = m_LastError;
            }
            return false;
        }

        lua_pushcfunction(m_State, Traceback);
        const int errorFunction = lua_gettop(m_State);
        int status = luaL_loadbufferx(m_State, source.data(), source.size(), "console", "t");
        if (status == LUA_OK)
        {
            status = lua_pcall(m_State, 0, 0, errorFunction);
        }
        if (status != LUA_OK)
        {
            const char* message = lua_tostring(m_State, -1);
            m_LastError = message != nullptr ? message : "Unknown Lua error";
            if (error != nullptr)
            {
                *error = m_LastError;
            }
            lua_settop(m_State, errorFunction - 1);
            return false;
        }

        lua_settop(m_State, errorFunction - 1);
        m_LastError.clear();
        return true;
    }
}
