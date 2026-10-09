#pragma once

#include <Aster/Scene/Components.h>
#include <Aster/Scene/Entity.h>

#include <cstddef>
#include <string>
#include <unordered_map>

namespace Aster
{
    class Scene
    {
    public:
        explicit Scene(std::string name = "Untitled");

        [[nodiscard]] Entity CreateEntity(std::string name = "Entity");
        [[nodiscard]] bool DestroyEntity(Entity entity) noexcept;
        [[nodiscard]] bool IsAlive(Entity entity) const noexcept;
        [[nodiscard]] const std::string* GetName(Entity entity) const noexcept;
        [[nodiscard]] TransformComponent* GetTransform(Entity entity) noexcept;
        [[nodiscard]] const TransformComponent* GetTransform(Entity entity) const noexcept;
        [[nodiscard]] const std::string& GetName() const noexcept;
        [[nodiscard]] std::size_t GetEntityCount() const noexcept;

    private:
        struct EntityData
        {
            std::string Name;
            TransformComponent Transform;
        };

        std::string m_Name;
        EntityId m_NextEntityId = 1;
        std::unordered_map<EntityId, EntityData> m_Entities;
    };
}