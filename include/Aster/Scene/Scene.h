#pragma once

#include "Aster/Assets/Asset.h"
#include "Aster/ECS/Registry.h"
#include "Aster/Scene/Components.h"

#include <glm/glm.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Aster
{
    struct SceneSettings
    {
        std::string Name = "Untitled";
        float Exposure = 1.0f;
        float AmbientIntensity = 0.15f;
        glm::vec3 AmbientColor{ 0.55f, 0.62f, 0.72f };
        bool EnableShadows = true;
        float ShadowStrength = 1.0f;
        uint32 ShadowMapSize = 2048;
        bool EnableSsao = true;
        float SsaoRadius = 0.5f;
        float SsaoIntensity = 1.0f;
        float SsaoBias = 0.025f;
        bool EnableBloom = false;
        float BloomThreshold = 1.0f;
        float BloomIntensity = 0.1f;
        bool EnableFxaa = true;
        bool ShowSkybox = true;
        std::string EnvironmentPath;
        float EnvironmentIntensity = 1.0f;
        glm::vec3 ClearColor{ 0.02f, 0.025f, 0.035f };
        bool VSync = true;
    };

    class Scene
    {
    public:
        explicit Scene(std::string name = "Untitled");

        [[nodiscard]] Entity CreateEntity(const std::string& name = "Entity");
        [[nodiscard]] Entity CreateEntity(const UUID& id, const std::string& name = "Entity");
        void DestroyEntity(Entity entity);
        void Clear();

        [[nodiscard]] bool IsAlive(Entity entity) const { return m_Registry.IsAlive(entity); }
        [[nodiscard]] Entity FindByUUID(const UUID& id) const;
        [[nodiscard]] Entity FindByName(std::string_view name) const;
        [[nodiscard]] std::vector<Entity> GetEntities() const { return m_Registry.GetAliveEntities(); }
        [[nodiscard]] size_t GetEntityCount() const { return m_Registry.GetEntityCount(); }
        [[nodiscard]] std::vector<Entity> GetRootEntities() const;

        void SetParent(Entity child, Entity parent, bool preserveWorldTransform = false);
        [[nodiscard]] Entity GetParent(Entity entity) const;
        [[nodiscard]] const std::vector<Entity>& GetChildren(Entity entity) const;
        [[nodiscard]] bool IsDescendantOf(Entity entity, Entity ancestor) const;

        [[nodiscard]] glm::mat4 GetWorldMatrix(Entity entity) const;
        [[nodiscard]] Transform GetWorldTransform(Entity entity) const;
        void SetWorldTransform(Entity entity, const Transform& worldTransform);

        [[nodiscard]] Entity InstantiateModel(const AssetRef<ModelAsset>& model, Entity parent = NullEntity);

        // Copies every entity from source into this scene. When parent is valid, all source roots are parented
        // to it; otherwise the copied roots retain no parent.
        void InstantiateScene(const Scene& source, Entity parent = NullEntity);

        // Copies one entity and its descendants from source into this scene.
        [[nodiscard]] Entity InstantiateEntity(const Scene& source, Entity sourceEntity,
            Entity parent = NullEntity);

        [[nodiscard]] Registry& GetRegistry() { return m_Registry; }
        [[nodiscard]] const Registry& GetRegistry() const { return m_Registry; }

        [[nodiscard]] SceneSettings& GetSettings() { return m_Settings; }
        [[nodiscard]] const SceneSettings& GetSettings() const { return m_Settings; }

        [[nodiscard]] Entity GetPrimaryCamera() const;

    private:
        void CopyEntityComponents(const Scene& source, Entity sourceEntity, Entity targetEntity);

        Registry m_Registry;
        SceneSettings m_Settings;
        std::unordered_map<UUID, Entity> m_Lookup;
    };
}
