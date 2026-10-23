#pragma once

#include "Aster/Core/Base.h"

#include <cstdarg>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

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

#define ASTER_LOG_TRACE(category, ...) ::Aster::Log::Write(::Aster::LogLevel::Trace, category, __VA_ARGS__)
#define ASTER_LOG_DEBUG(category, ...) ::Aster::Log::Write(::Aster::LogLevel::Debug, category, __VA_ARGS__)
#define ASTER_LOG_INFO(category, ...) ::Aster::Log::Write(::Aster::LogLevel::Info, category, __VA_ARGS__)
#define ASTER_LOG_WARN(category, ...) ::Aster::Log::Write(::Aster::LogLevel::Warning, category, __VA_ARGS__)
#define ASTER_LOG_ERROR(category, ...) ::Aster::Log::Write(::Aster::LogLevel::Error, category, __VA_ARGS__)
#define ASTER_LOG_CRITICAL(category, ...) ::Aster::Log::Write(::Aster::LogLevel::Critical, category, __VA_ARGS__)
