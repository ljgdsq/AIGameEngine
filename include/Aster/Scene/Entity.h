#pragma once

#include <cstdint>

namespace Aster
{
    using EntityId = std::uint64_t;

    struct Entity
    {
        EntityId Id = 0;

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return Id != 0;
        }

        friend constexpr bool operator==(Entity, Entity) = default;
    };
}