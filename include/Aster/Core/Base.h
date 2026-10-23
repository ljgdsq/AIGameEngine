#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace Aster
{
    using int8 = std::int8_t;
    using int16 = std::int16_t;
    using int32 = std::int32_t;
    using int64 = std::int64_t;
    using uint8 = std::uint8_t;
    using uint16 = std::uint16_t;
    using uint32 = std::uint32_t;
    using uint64 = std::uint64_t;

    template<typename T>
    using Scope = std::unique_ptr<T>;

    template<typename T, typename... Args>
    [[nodiscard]] Scope<T> CreateScope(Args&&... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    using Ref = std::shared_ptr<T>;

    template<typename T, typename... Args>
    [[nodiscard]] Ref<T> CreateRef(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    [[noreturn]] void AssertFailed(const char* expression, const char* message, const char* file, int line);

    constexpr std::string_view AsterVersion = "0.2.0";
}

#if defined(ASTER_DEBUG) || defined(_DEBUG)
#define ASTER_ASSERT(expression, message)                                                          \
    do                                                                                             \
    {                                                                                              \
        if (!(expression))                                                                         \
        {                                                                                          \
            ::Aster::AssertFailed(#expression, message, __FILE__, __LINE__);                      \
        }                                                                                          \
    } while (false)
#else
#define ASTER_ASSERT(expression, message) ((void)0)
#endif

#define ASTER_VERIFY(expression) ASTER_ASSERT(expression, "verification failed")
