#pragma once

#include "Aster/Core/Base.h"

#include <cstdarg>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

// Severity: Error, Warning, Info and Debug are the supported runtime levels. Trace is an
// additional verbose level and Critical marks unrecoverable failures.
//
// ASTER_ENABLE_DEBUG_LOGS is normally supplied by CMake. When a translation unit is compiled
// without that definition (for example by a third-party consumer), fall back to the standard
// Release (NDEBUG) configuration so that Debug and Trace statements never reach a release build.
#ifndef ASTER_ENABLE_DEBUG_LOGS
#if defined(NDEBUG)
#define ASTER_ENABLE_DEBUG_LOGS 0
#else
#define ASTER_ENABLE_DEBUG_LOGS 1
#endif
#endif

namespace Aster
{
    enum class LogLevel : uint8
    {
        Trace = 0,
        Debug,
        Info,
        Warning,
        Error,
        Critical,
        Off
    };

    struct LogMessage
    {
        LogLevel Level = LogLevel::Info;
        std::string Category;
        std::string Text;
    };

    class Log
    {
    public:
        using Sink = std::function<void(const LogMessage&)>;
        using SinkId = uint64;

        static void Initialize();
        static void Shutdown();

        static void SetLevel(LogLevel level);
        [[nodiscard]] static LogLevel GetLevel();

        static void Write(LogLevel level, std::string_view category, const char* format, ...);

        [[nodiscard]] static std::vector<LogMessage> GetMessages();
        static void Clear();

        static SinkId AddSink(Sink sink);
        static void RemoveSink(SinkId id);

        [[nodiscard]] static std::string_view LevelToString(LogLevel level);
    };
}

#if ASTER_ENABLE_DEBUG_LOGS
#define ASTER_LOG_TRACE(category, ...) ::Aster::Log::Write(::Aster::LogLevel::Trace, category, __VA_ARGS__)
#define ASTER_LOG_DEBUG(category, ...) ::Aster::Log::Write(::Aster::LogLevel::Debug, category, __VA_ARGS__)
#else
// Debug/Trace statements disappear completely in release builds: the arguments are not
// evaluated and no formatting or locking happens. The expressions remain valid statements so
// they can still be used in unbraced if/else bodies.
#define ASTER_LOG_TRACE(category, ...) ((void)0)
#define ASTER_LOG_DEBUG(category, ...) ((void)0)
#endif
#define ASTER_LOG_INFO(category, ...) ::Aster::Log::Write(::Aster::LogLevel::Info, category, __VA_ARGS__)
#define ASTER_LOG_WARN(category, ...) ::Aster::Log::Write(::Aster::LogLevel::Warning, category, __VA_ARGS__)
#define ASTER_LOG_ERROR(category, ...) ::Aster::Log::Write(::Aster::LogLevel::Error, category, __VA_ARGS__)
#define ASTER_LOG_CRITICAL(category, ...) ::Aster::Log::Write(::Aster::LogLevel::Critical, category, __VA_ARGS__)
