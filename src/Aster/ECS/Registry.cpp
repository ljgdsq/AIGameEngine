#include "Aster/ECS/Registry.h"

#include <limits>

namespace Aster
{
    Entity Registry::Create()
    {
        uint32 index = 0;
        if (!m_FreeIndices.empty())
        {
            index = m_FreeIndices.back();
            m_FreeIndices.pop_back();
            EntityRecord& record = m_Entities[index];
            record.Alive = true;
            ++m_AliveCount;
            return Entity::FromIndex(index, record.Generation);
        }

        index = static_cast<uint32>(m_Entities.size());
        if (m_Entities.empty())
        {
            // Reserve index 0 as the invalid entity.
            m_Entities.push_back(EntityRecord{ 0, false });
            index = 1;
        }

        EntityRecord record;
        record.Generation = 1;
        record.Alive = true;
        if (index == m_Entities.size())
        {
            m_Entities.push_back(record);
        }
        else
        {
            m_Entities[index] = record;
        }
        ++m_AliveCount;
        return Entity::FromIndex(index, record.Generation);
    }

    bool Registry::Destroy(Entity entity)
    {
        if (!IsAlive(entity))
        {
            return false;
        }

        for (auto& [type, pool] : m_Pools)
        {
            (void)type;
            pool->Remove(entity.Index());
        }

        EntityRecord& record = m_Entities[entity.Index()];
        record.Alive = false;
        if (record.Generation == std::numeric_limits<uint32>::max())
        {
            record.Generation = 1;
        }
        else
        {
            ++record.Generation;
        }
        m_FreeIndices.push_back(entity.Index());
        ASTER_ASSERT(m_AliveCount > 0, "Alive count underflow");
        --m_AliveCount;
        return true;
    }

    bool Registry::IsAlive(Entity entity) const
    {
        if (entity.IsNull())
        {
            return false;
        }
        const uint32 index = entity.Index();
        if (index == 0 || index >= m_Entities.size())
        {
            return false;
        }
        const EntityRecord& record = m_Entities[index];
        return record.Alive && record.Generation == entity.Generation();
    }

    void Registry::Clear()
    {
        for (auto& [type, pool] : m_Pools)
        {
            (void)type;
            pool->Clear();
        }
        m_Entities.clear();
        m_FreeIndices.clear();
        m_AliveCount = 0;
    }

    std::vector<Entity> Registry::GetAliveEntities() const
    {
        std::vector<Entity> entities;
        entities.reserve(m_AliveCount);
        for (size_t index = 1; index < m_Entities.size(); ++index)
        {
            if (m_Entities[index].Alive)
            {
                entities.push_back(Entity::FromIndex(static_cast<uint32>(index), m_Entities[index].Generation));
            }
        }
        return entities;
    }
}
