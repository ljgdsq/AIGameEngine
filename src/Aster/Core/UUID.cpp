#include "Aster/Core/UUID.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <mutex>
#include <random>

namespace Aster
{
    namespace
    {
        std::mutex g_RandomMutex;
        std::mt19937_64& RandomEngine()
        {
            static std::mt19937_64 engine = []
            {
                std::random_device device;
                const uint64 seed = (static_cast<uint64>(device()) << 32U) ^ static_cast<uint64>(device());
                return std::mt19937_64(seed);
            }();
            return engine;
        }

        uint64 NextRandom()
        {
            std::lock_guard lock(g_RandomMutex);
            return RandomEngine()();
        }

        int HexValue(char character)
        {
            if (character >= '0' && character <= '9') return character - '0';
            if (character >= 'a' && character <= 'f') return character - 'a' + 10;
            if (character >= 'A' && character <= 'F') return character - 'A' + 10;
            return -1;
        }
    }

    UUID UUID::Generate()
    {
        UUID result;
        result.High = NextRandom();
        result.Low = NextRandom();

        // RFC 4122 version 4, variant 1 nibble.
        result.High = (result.High & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
        result.Low = (result.Low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
        if (result.IsNil())
        {
            result.Low = 1;
        }
        return result;
    }

    UUID UUID::Nil()
    {
        return UUID();
    }

    std::optional<UUID> UUID::FromString(std::string_view text)
    {
        std::array<char, 32> digits{};
        size_t count = 0;
        for (char character : text)
        {
            if (character == '-')
            {
                continue;
            }
            if (!std::isxdigit(static_cast<unsigned char>(character)))
            {
                return std::nullopt;
            }
            if (count >= digits.size())
            {
                return std::nullopt;
            }
            digits[count++] = character;
        }

        if (count != digits.size())
        {
            return std::nullopt;
        }

        uint64 high = 0;
        uint64 low = 0;
        for (size_t index = 0; index < 16; ++index)
        {
            high = (high << 4U) | static_cast<uint64>(HexValue(digits[index]));
        }
        for (size_t index = 16; index < 32; ++index)
        {
            low = (low << 4U) | static_cast<uint64>(HexValue(digits[index]));
        }

        UUID result(high, low);
        if (result.IsNil())
        {
            return std::nullopt;
        }
        return result;
    }

    std::string UUID::ToString() const
    {
        char buffer[37];
        std::snprintf(buffer, sizeof(buffer), "%08llx-%04llx-%04llx-%04llx-%012llx",
            static_cast<unsigned long long>((High >> 32U) & 0xFFFFFFFFULL),
            static_cast<unsigned long long>((High >> 16U) & 0xFFFFULL),
            static_cast<unsigned long long>(High & 0xFFFFULL),
            static_cast<unsigned long long>((Low >> 48U) & 0xFFFFULL),
            static_cast<unsigned long long>(Low & 0xFFFFFFFFFFFFULL));
        return std::string(buffer);
    }
}
