#include <Aster/Scene/Scene.h>

#include <utility>

namespace Aster
{
    Scene::Scene(std::string name)
        : m_Name(std::move(name))
    {
    }

    Entity Scene::CreateEntity(std::string name)
    {
        const Entity entity{ m_NextEntityId++ };
        m_Entities.emplace(entity.Id, EntityData{ std::move(name), {} });
        return entity;
    }

    bool Scene::DestroyEntity(Entity entity) noexcept
    {
        return m_Entities.erase(entity.Id) != 0;
    }

    bool Scene::IsAlive(Entity entity) const noexcept
    {
        return m_Entities.contains(entity.Id);
    }

    const std::string* Scene::GetName(Entity entity) const noexcept
    {
        const auto iterator = m_Entities.find(entity.Id);
        return iterator == m_Entities.end() ? nullptr : &iterator->second.Name;
    }

    TransformComponent* Scene::GetTransform(Entity entity) noexcept
    {
        const auto iterator = m_Entities.find(entity.Id);
        return iterator == m_Entities.end() ? nullptr : &iterator->second.Transform;
    }

    const TransformComponent* Scene::GetTransform(Entity entity) const noexcept
    {
        const auto iterator = m_Entities.find(entity.Id);
        return iterator == m_Entities.end() ? nullptr : &iterator->second.Transform;
    }

    const std::string& Scene::GetName() const noexcept
    {
        return m_Name;
    }

    std::size_t Scene::GetEntityCount() const noexcept
    {
        return m_Entities.size();
    }
}