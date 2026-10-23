#pragma once

#include "Aster/Core/Base.h"
#include "Aster/Core/Log.h"
#include "Aster/ECS/Entity.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Aster
{
    class IComponentPool
    {
    public:
        virtual ~IComponentPool() = default;
        virtual void Remove(uint32 index) = 0;
        [[nodiscard]] virtual bool Has(uint32 index) const = 0;
        virtual void Clear() = 0;
        [[nodiscard]] virtual size_t Size() const = 0;
    };

    template<typename T>
    class ComponentPool final : public IComponentPool
    {
    public:
        template<typename... Args>
        T& Emplace(uint32 index, Args&&... args)
        {
            if (m_Components.size() <= index)
            {
                m_Components.resize(static_cast<size_t>(index) + 1);
            }
            m_Components[index].reset();
            m_Components[index].emplace(std::forward<Args>(args)...);
            ++m_Size;
            return *m_Components[index];
        }

        void Remove(uint32 index) override
        {
            if (index < m_Components.size() && m_Components[index].has_value())
            {
                m_Components[index].reset();
                --m_Size;
            }
        }

        [[nodiscard]] bool Has(uint32 index) const override
        {
            return index < m_Components.size() && m_Components[index].has_value();
        }

        [[nodiscard]] T* TryGet(uint32 index)
        {
            if (!Has(index))
            {
                return nullptr;
            }
            return &*m_Components[index];
        }

        [[nodiscard]] const T* TryGet(uint32 index) const
        {
            if (!Has(index))
            {
                return nullptr;
            }
            return &*m_Components[index];
        }

        void Clear() override
        {
            m_Components.clear();
            m_Size = 0;
        }

        [[nodiscard]] size_t Size() const override { return m_Size; }

        template<typename Function>
        void ForEach(Function&& function)
        {
            for (size_t index = 0; index < m_Components.size(); ++index)
            {
                if (m_Components[index].has_value())
                {
                    function(static_cast<uint32>(index), *m_Components[index]);
                }
            }
        }

    private:
        std::vector<std::optional<T>> m_Components;
        size_t m_Size = 0;
    };

    class Registry
    {
    public:
        Registry() = default;
        Registry(const Registry&) = delete;
        Registry& operator=(const Registry&) = delete;
        Registry(Registry&&) noexcept = default;
        Registry& operator=(Registry&&) noexcept = default;
        ~Registry() = default;

        [[nodiscard]] Entity Create();
        bool Destroy(Entity entity);
        [[nodiscard]] bool IsAlive(Entity entity) const;
        void Clear();

        template<typename T, typename... Args>
        T& Emplace(Entity entity, Args&&... args)
        {
            ASTER_ASSERT(IsAlive(entity), "Cannot add a component to a dead entity");
            ComponentPool<T>& pool = GetOrCreatePool<T>();
            auto& returnValue = pool.Emplace(entity.Index(), std::forward<Args>(args)...);
            return returnValue;
        }

        template<typename T>
        void Remove(Entity entity)
        {
            if (!IsAlive(entity))
            {
                return;
            }
            auto iterator = m_Pools.find(std::type_index(typeid(T)));
            if (iterator != m_Pools.end())
            {
                iterator->second->Remove(entity.Index());
            }
        }

        template<typename T>
        [[nodiscard]] bool Has(Entity entity) const
        {
            if (!IsAlive(entity))
            {
                return false;
            }
            auto iterator = m_Pools.find(std::type_index(typeid(T)));
            if (iterator == m_Pools.end())
            {
                return false;
            }
            return iterator->second->Has(entity.Index());
        }

        template<typename T>
        [[nodiscard]] T* TryGet(Entity entity)
        {
            if (!IsAlive(entity))
            {
                return nullptr;
            }
            ComponentPool<T>* pool = TryGetPool<T>();
            if (pool == nullptr)
            {
                return nullptr;
            }
            return pool->TryGet(entity.Index());
        }

        template<typename T>
        [[nodiscard]] const T* TryGet(Entity entity) const
        {
            if (!IsAlive(entity))
            {
                return nullptr;
            }
            auto iterator = m_Pools.find(std::type_index(typeid(T)));
            if (iterator == m_Pools.end())
            {
                return nullptr;
            }
            const auto* pool = dynamic_cast<const ComponentPool<T>*>(iterator->second.get());
            if (pool == nullptr)
            {
                return nullptr;
            }
            return pool->TryGet(entity.Index());
        }

        template<typename T>
        [[nodiscard]] T& Get(Entity entity)
        {
            T* component = TryGet<T>(entity);
            ASTER_ASSERT(component != nullptr, "Entity is missing the requested component");
            return *component;
        }

        template<typename T>
        [[nodiscard]] const T& Get(Entity entity) const
        {
            const T* component = TryGet<T>(entity);
            ASTER_ASSERT(component != nullptr, "Entity is missing the requested component");
            return *component;
        }

        [[nodiscard]] std::vector<Entity> GetAliveEntities() const;
        [[nodiscard]] size_t GetEntityCount() const { return m_AliveCount; }
        [[nodiscard]] size_t GetComponentPoolCount() const { return m_Pools.size(); }

        template<typename... Ts, typename Function>
        void Each(Function&& function)
        {
            for (const Entity entity : GetAliveEntities())
            {
                if (HasAll<Ts...>(entity))
                {
                    std::invoke(function, entity, Get<Ts>(entity)...);
                }
            }
        }

    private:
        struct EntityRecord
        {
            uint32 Generation = 0;
            bool Alive = false;
        };

        template<typename T>
        ComponentPool<T>& GetOrCreatePool()
        {
            const std::type_index type(typeid(T));
            auto iterator = m_Pools.find(type);
            if (iterator == m_Pools.end())
            {
                auto pool = std::make_unique<ComponentPool<T>>();
                ComponentPool<T>& reference = *pool;
                m_Pools.emplace(type, std::move(pool));
                return reference;
            }
            return *static_cast<ComponentPool<T>*>(iterator->second.get());
        }

        template<typename T>
        ComponentPool<T>* TryGetPool()
        {
            auto iterator = m_Pools.find(std::type_index(typeid(T)));
            if (iterator == m_Pools.end())
            {
                return nullptr;
            }
            return static_cast<ComponentPool<T>*>(iterator->second.get());
        }

        template<typename... Ts>
        [[nodiscard]] bool HasAll(Entity entity) const
        {
            return (Has<Ts>(entity) && ...);
        }

        std::vector<EntityRecord> m_Entities;
        std::vector<uint32> m_FreeIndices;
        std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> m_Pools;
        size_t m_AliveCount = 0;
    };
}
