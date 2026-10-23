#pragma once

#include "Aster/Core/Base.h"

#include <compare>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace Aster
{
    class UUID
    {
    public:
        UUID() = default;
        UUID(uint64 high, uint64 low) : High(high), Low(low) { }

        [[nodiscard]] static UUID Generate();
        [[nodiscard]] static UUID Nil();
        [[nodiscard]] static std::optional<UUID> FromString(std::string_view text);
        [[nodiscard]] std::string ToString() const;

        [[nodiscard]] bool IsNil() const { return High == 0 && Low == 0; }
        [[nodiscard]] bool IsValid() const { return !IsNil(); }

        [[nodiscard]] uint64 GetHigh() const { return High; }
        [[nodiscard]] uint64 GetLow() const { return Low; }

        auto operator<=>(const UUID&) const = default;
        bool operator==(const UUID&) const = default;

        uint64 High = 0;
        uint64 Low = 0;
    };
}

namespace std
{
    template<>
    struct hash<Aster::UUID>
    {
        size_t operator()(const Aster::UUID& value) const noexcept
        {
            size_t seed = std::hash<Aster::uint64>{}(value.High);
            seed ^= std::hash<Aster::uint64>{}(value.Low) + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
            return seed;
        }
    };
}
