#pragma once

#include "Aster/Core/Base.h"

#include <compare>
#include <functional>
#include <string>

namespace Aster
{
    class Entity
    {
    public:
        using IdType = uint64;

        constexpr Entity() = default;

        [[nodiscard]] static constexpr Entity FromIndex(uint32 index, uint32 generation)
        {
            return Entity((static_cast<uint64>(generation) << 32U) | static_cast<uint64>(index));
        }

        [[nodiscard]] static constexpr Entity Null() { return Entity(0); }

        [[nodiscard]] constexpr uint32 Index() const { return static_cast<uint32>(m_Id & 0xFFFFFFFFULL); }
        [[nodiscard]] constexpr uint32 Generation() const { return static_cast<uint32>(m_Id >> 32U); }
        [[nodiscard]] constexpr uint64 Raw() const { return m_Id; }
        [[nodiscard]] constexpr bool IsNull() const { return m_Id == 0; }
        [[nodiscard]] constexpr explicit operator bool() const { return m_Id != 0; }

        auto operator<=>(const Entity&) const = default;
        bool operator==(const Entity&) const = default;

        [[nodiscard]] std::string ToString() const
        {
            return std::to_string(Index()) + ":" + std::to_string(Generation());
        }

    private:
        explicit constexpr Entity(uint64 id) : m_Id(id) { }

        uint64 m_Id = 0;
    };

    inline constexpr Entity NullEntity = Entity::Null();
}

namespace std
{
    template<>
    struct hash<Aster::Entity>
    {
        size_t operator()(const Aster::Entity& entity) const noexcept
        {
            return std::hash<Aster::uint64>{}(entity.Raw());
        }
    };
}
