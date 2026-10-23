#include "Aster/Scene/Scene.h"

#include "Aster/Core/Log.h"

#include <algorithm>
#include <functional>
#include <unordered_set>

namespace Aster
{
    namespace
    {
        Entity FindPrimaryCamera(const Scene& scene)
        {
            for (Entity entity : scene.GetEntities())
            {
                if (scene.GetRegistry().Has<CameraComponent>(entity) &&
                    scene.GetRegistry().Get<CameraComponent>(entity).Primary)
                {
                    return entity;
                }
            }
            return NullEntity;
        }
    }

    Scene::Scene(std::string name)
    {
        m_Settings.Name = std::move(name);
    }

    Entity Scene::CreateEntity(const std::string& name)
    {
        return CreateEntity(UUID::Generate(), name);
    }

    Entity Scene::CreateEntity(const UUID& id, const std::string& name)
    {
        UUID finalId = id;
        if (finalId.IsNil())
        {
            finalId = UUID::Generate();
        }
        if (m_Lookup.contains(finalId))
        {
            ASTER_LOG_WARN("Scene", "Duplicate entity UUID '%s'; generating a new one", finalId.ToString().c_str());
            finalId = UUID::Generate();
        }

        const Entity entity = m_Registry.Create();
        m_Registry.Emplace<IDComponent>(entity, IDComponent{ finalId });
        m_Registry.Emplace<NameComponent>(entity, NameComponent{ name });
        m_Registry.Emplace<TransformComponent>(entity);
        m_Registry.Emplace<RelationshipComponent>(entity);
        m_Lookup.emplace(finalId, entity);
        return entity;
    }

    void Scene::DestroyEntity(Entity entity)
    {
        if (!m_Registry.IsAlive(entity))
        {
            return;
        }

        std::vector<Entity> children;
        if (m_Registry.Has<RelationshipComponent>(entity))
        {
            children = m_Registry.Get<RelationshipComponent>(entity).Children;
        }
        for (Entity child : children)
        {
            DestroyEntity(child);
        }

        const Entity parent = GetParent(entity);
        if (parent != NullEntity && m_Registry.IsAlive(parent) && m_Registry.Has<RelationshipComponent>(parent))
        {
            auto& parentChildren = m_Registry.Get<RelationshipComponent>(parent).Children;
            parentChildren.erase(std::remove(parentChildren.begin(), parentChildren.end(), entity), parentChildren.end());
        }

        if (m_Registry.Has<IDComponent>(entity))
        {
            m_Lookup.erase(m_Registry.Get<IDComponent>(entity).Id);
        }
        m_Registry.Destroy(entity);
    }

    void Scene::Clear()
    {
        m_Registry.Clear();
        m_Lookup.clear();
    }

    Entity Scene::FindByUUID(const UUID& id) const
    {
        const auto iterator = m_Lookup.find(id);
        return iterator != m_Lookup.end() && m_Registry.IsAlive(iterator->second) ? iterator->second : NullEntity;
    }

    Entity Scene::FindByName(std::string_view name) const
    {
        for (Entity entity : m_Registry.GetAliveEntities())
        {
            if (m_Registry.Has<NameComponent>(entity) && m_Registry.Get<NameComponent>(entity).Name == name)
            {
                return entity;
            }
        }
        return NullEntity;
    }

    std::vector<Entity> Scene::GetRootEntities() const
    {
        std::vector<Entity> roots;
        for (Entity entity : m_Registry.GetAliveEntities())
        {
            if (GetParent(entity) == NullEntity)
            {
                roots.push_back(entity);
            }
        }
        return roots;
    }

    void Scene::SetParent(Entity child, Entity parent, bool preserveWorldTransform)
    {
        if (!m_Registry.IsAlive(child) || (parent != NullEntity && !m_Registry.IsAlive(parent)) || child == parent)
        {
            return;
        }
        if (parent != NullEntity && IsDescendantOf(parent, child))
        {
            ASTER_LOG_WARN("Scene", "Refusing to parent an entity to one of its descendants");
            return;
        }

        const Transform worldBefore = preserveWorldTransform ? GetWorldTransform(child) : Transform{};

        const Entity oldParent = GetParent(child);
        if (oldParent != NullEntity && m_Registry.Has<RelationshipComponent>(oldParent))
        {
            auto& children = m_Registry.Get<RelationshipComponent>(oldParent).Children;
            children.erase(std::remove(children.begin(), children.end(), child), children.end());
        }

        if (m_Registry.Has<RelationshipComponent>(child))
        {
            m_Registry.Get<RelationshipComponent>(child).Parent = parent;
        }
        if (parent != NullEntity && m_Registry.Has<RelationshipComponent>(parent))
        {
            auto& children = m_Registry.Get<RelationshipComponent>(parent).Children;
            if (std::find(children.begin(), children.end(), child) == children.end())
            {
                children.push_back(child);
            }
        }

        if (preserveWorldTransform)
        {
            SetWorldTransform(child, worldBefore);
        }
    }

    Entity Scene::GetParent(Entity entity) const
    {
        if (!m_Registry.IsAlive(entity) || !m_Registry.Has<RelationshipComponent>(entity))
        {
            return NullEntity;
        }
        const Entity parent = m_Registry.Get<RelationshipComponent>(entity).Parent;
        return m_Registry.IsAlive(parent) ? parent : NullEntity;
    }

    const std::vector<Entity>& Scene::GetChildren(Entity entity) const
    {
        static const std::vector<Entity> empty;
        if (!m_Registry.IsAlive(entity) || !m_Registry.Has<RelationshipComponent>(entity))
        {
            return empty;
        }
        return m_Registry.Get<RelationshipComponent>(entity).Children;
    }

    bool Scene::IsDescendantOf(Entity entity, Entity ancestor) const
    {
        if (entity == NullEntity || ancestor == NullEntity)
        {
            return false;
        }
        Entity current = GetParent(entity);
        size_t guard = 0;
        while (current != NullEntity && guard++ < 256)
        {
            if (current == ancestor)
            {
                return true;
            }
            current = GetParent(current);
        }
        return false;
    }

    glm::mat4 Scene::GetWorldMatrix(Entity entity) const
    {
        if (!m_Registry.IsAlive(entity) || !m_Registry.Has<TransformComponent>(entity))
        {
            return glm::mat4(1.0f);
        }

        glm::mat4 world = m_Registry.Get<TransformComponent>(entity).Local.GetMatrix();
        Entity parent = GetParent(entity);
        size_t guard = 0;
        while (parent != NullEntity && guard++ < 256)
        {
            if (m_Registry.Has<TransformComponent>(parent))
            {
                world = m_Registry.Get<TransformComponent>(parent).Local.GetMatrix() * world;
            }
            parent = GetParent(parent);
        }
        return world;
    }

    Transform Scene::GetWorldTransform(Entity entity) const
    {
        Transform result;
        if (!DecomposeTransform(GetWorldMatrix(entity), result))
        {
            if (m_Registry.Has<TransformComponent>(entity))
            {
                return m_Registry.Get<TransformComponent>(entity).Local;
            }
        }
        return result;
    }

    void Scene::SetWorldTransform(Entity entity, const Transform& worldTransform)
    {
        if (!m_Registry.IsAlive(entity) || !m_Registry.Has<TransformComponent>(entity))
        {
            return;
        }

        const Entity parent = GetParent(entity);
        if (parent == NullEntity)
        {
            m_Registry.Get<TransformComponent>(entity).Local = worldTransform;
            return;
        }

        const glm::mat4 parentWorld = GetWorldMatrix(parent);
        const glm::mat4 local = glm::inverse(parentWorld) * worldTransform.GetMatrix();
        Transform decomposed;
        if (DecomposeTransform(local, decomposed))
        {
            m_Registry.Get<TransformComponent>(entity).Local = decomposed;
        }
    }

    Entity Scene::InstantiateModel(const AssetRef<ModelAsset>& model, Entity parent)
    {
        if (!model.IsValid())
        {
            return NullEntity;
        }

        Entity firstRoot = NullEntity;
        std::function<Entity(const ModelNode&, Entity)> instantiateNode = [&](const ModelNode& node,
            Entity nodeParent) -> Entity
        {
            const Entity entity = CreateEntity(node.Name.empty() ? "Model Node" : node.Name);
            if (nodeParent != NullEntity)
            {
                SetParent(entity, nodeParent);
            }
            m_Registry.Get<TransformComponent>(entity).Local = node.LocalTransform;

            if (node.Mesh)
            {
                MeshRendererComponent renderer;
                renderer.Mesh = AssetRef<MeshAsset>(node.Mesh, node.Mesh->SourcePath);
                renderer.Materials = node.Mesh->Materials;
                if (renderer.Materials.empty())
                {
                    renderer.Materials.push_back(Material{});
                }
                m_Registry.Emplace<MeshRendererComponent>(entity, std::move(renderer));
            }

            for (const ModelNode& child : node.Children)
            {
                instantiateNode(child, entity);
            }
            return entity;
        };

        for (const ModelNode& root : model->Roots)
        {
            const Entity entity = instantiateNode(root, parent);
            if (firstRoot == NullEntity)
            {
                firstRoot = entity;
            }
        }
        return firstRoot;
    }

    void Scene::InstantiateScene(const Scene& source, Entity parent)
    {
        if (&source == this)
        {
            ASTER_LOG_WARN("Scene", "InstantiateScene cannot copy a scene into itself");
            return;
        }

        std::unordered_map<Entity, Entity> mapping;
        mapping.reserve(source.GetEntityCount());
        for (Entity sourceEntity : source.GetEntities())
        {
            std::string name = "Entity";
            if (source.m_Registry.Has<NameComponent>(sourceEntity))
            {
                name = source.m_Registry.Get<NameComponent>(sourceEntity).Name;
            }
            const Entity target = CreateEntity(name);
            mapping.emplace(sourceEntity, target);
            CopyEntityComponents(source, sourceEntity, target);
        }

        for (const auto& [sourceEntity, targetEntity] : mapping)
        {
            const Entity sourceParent = source.GetParent(sourceEntity);
            if (sourceParent != NullEntity)
            {
                const auto iterator = mapping.find(sourceParent);
                if (iterator != mapping.end())
                {
                    SetParent(targetEntity, iterator->second);
                }
            }
            else if (parent != NullEntity)
            {
                SetParent(targetEntity, parent);
            }
        }
    }

    Entity Scene::InstantiateEntity(const Scene& source, Entity sourceEntity, Entity parent)
    {
        if (&source == this || !source.m_Registry.IsAlive(sourceEntity))
        {
            return NullEntity;
        }

        std::string name = "Entity";
        if (source.m_Registry.Has<NameComponent>(sourceEntity))
        {
            name = source.m_Registry.Get<NameComponent>(sourceEntity).Name;
        }
        const Entity target = CreateEntity(name);
        CopyEntityComponents(source, sourceEntity, target);
        if (parent != NullEntity)
        {
            SetParent(target, parent);
        }

        for (Entity child : source.GetChildren(sourceEntity))
        {
            (void)InstantiateEntity(source, child, target);
        }
        return target;
    }

    void Scene::CopyEntityComponents(const Scene& source, Entity sourceEntity, Entity targetEntity)
    {
        const Registry& registry = source.m_Registry;
        if (registry.Has<NameComponent>(sourceEntity))
        {
            m_Registry.Emplace<NameComponent>(targetEntity, registry.Get<NameComponent>(sourceEntity));
        }
        if (registry.Has<TransformComponent>(sourceEntity))
        {
            m_Registry.Emplace<TransformComponent>(targetEntity, registry.Get<TransformComponent>(sourceEntity));
        }
        if (registry.Has<CameraComponent>(sourceEntity))
        {
            m_Registry.Emplace<CameraComponent>(targetEntity, registry.Get<CameraComponent>(sourceEntity));
        }
        if (registry.Has<DirectionalLightComponent>(sourceEntity))
        {
            m_Registry.Emplace<DirectionalLightComponent>(targetEntity, registry.Get<DirectionalLightComponent>(sourceEntity));
        }
        if (registry.Has<PointLightComponent>(sourceEntity))
        {
            m_Registry.Emplace<PointLightComponent>(targetEntity, registry.Get<PointLightComponent>(sourceEntity));
        }
        if (registry.Has<SpotLightComponent>(sourceEntity))
        {
            m_Registry.Emplace<SpotLightComponent>(targetEntity, registry.Get<SpotLightComponent>(sourceEntity));
        }
        if (registry.Has<MeshRendererComponent>(sourceEntity))
        {
            m_Registry.Emplace<MeshRendererComponent>(targetEntity, registry.Get<MeshRendererComponent>(sourceEntity));
        }
        if (registry.Has<RigidBodyComponent>(sourceEntity))
        {
            m_Registry.Emplace<RigidBodyComponent>(targetEntity, registry.Get<RigidBodyComponent>(sourceEntity));
        }
        if (registry.Has<BoxColliderComponent>(sourceEntity))
        {
            m_Registry.Emplace<BoxColliderComponent>(targetEntity, registry.Get<BoxColliderComponent>(sourceEntity));
        }
        if (registry.Has<SphereColliderComponent>(sourceEntity))
        {
            m_Registry.Emplace<SphereColliderComponent>(targetEntity, registry.Get<SphereColliderComponent>(sourceEntity));
        }
        if (registry.Has<ScriptComponent>(sourceEntity))
        {
            m_Registry.Emplace<ScriptComponent>(targetEntity, registry.Get<ScriptComponent>(sourceEntity));
        }
        if (registry.Has<AudioSourceComponent>(sourceEntity))
        {
            m_Registry.Emplace<AudioSourceComponent>(targetEntity, registry.Get<AudioSourceComponent>(sourceEntity));
        }
        if (registry.Has<AudioListenerComponent>(sourceEntity))
        {
            m_Registry.Emplace<AudioListenerComponent>(targetEntity, registry.Get<AudioListenerComponent>(sourceEntity));
        }
    }

    Entity Scene::GetPrimaryCamera() const
    {
        return FindPrimaryCamera(*this);
    }
}
