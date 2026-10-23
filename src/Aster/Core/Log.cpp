#include "Aster/Core/Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#if defined(_WIN32)
#include <spdlog/sinks/msvc_sink.h>
#endif

#include <algorithm>
#include <cstdio>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Aster
{
    namespace
    {
        struct LogState
        {
            std::mutex Mutex;
            LogLevel Level = LogLevel::Trace;
            std::deque<LogMessage> Messages;
            std::unordered_map<Log::SinkId, Log::Sink> Sinks;
            std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> Loggers;
            std::vector<spdlog::sink_ptr> SpdlogSinks;
            std::shared_ptr<spdlog::sinks::sink> Bridge;
            Log::SinkId NextSinkId = 1;
            bool Initialized = false;
        };

        LogState& State()
        {
            static LogState state;
            return state;
        }

        constexpr size_t MaxMessages = 2000;
        constexpr size_t MaxMessageLength = 4096;

        spdlog::level::level_enum ToSpdlogLevel(LogLevel level)
        {
            switch (level)
            {
                case LogLevel::Trace: return spdlog::level::trace;
                case LogLevel::Debug: return spdlog::level::debug;
                case LogLevel::Info: return spdlog::level::info;
                case LogLevel::Warning: return spdlog::level::warn;
                case LogLevel::Error: return spdlog::level::err;
                case LogLevel::Critical: return spdlog::level::critical;
                case LogLevel::Off: return spdlog::level::off;
            }
            return spdlog::level::info;
        }

        LogLevel FromSpdlogLevel(spdlog::level::level_enum level)
        {
            switch (level)
            {
                case spdlog::level::trace: return LogLevel::Trace;
                case spdlog::level::debug: return LogLevel::Debug;
                case spdlog::level::info: return LogLevel::Info;
                case spdlog::level::warn: return LogLevel::Warning;
                case spdlog::level::err: return LogLevel::Error;
                case spdlog::level::critical: return LogLevel::Critical;
                default: return LogLevel::Off;
            }
        }

        void Publish(LogMessage message)
        {
            std::vector<Log::Sink> sinks;
            {
                std::lock_guard lock(State().Mutex);
                if (!State().Initialized || State().Level == LogLevel::Off ||
                    message.Level < State().Level)
                {
                    return;
                }

                State().Messages.push_back(message);
                while (State().Messages.size() > MaxMessages)
                {
                    State().Messages.pop_front();
                }

                sinks.reserve(State().Sinks.size());
                for (const auto& [id, sink] : State().Sinks)
                {
                    (void)id;
                    sinks.push_back(sink);
                }
            }

            for (const Log::Sink& sink : sinks)
            {
                if (sink)
                {
                    sink(message);
                }
            }
        }

        class BridgeSink final : public spdlog::sinks::base_sink<std::mutex>
        {
        protected:
            void sink_it_(const spdlog::details::log_msg& message) override
            {
                LogMessage output;
                output.Level = FromSpdlogLevel(message.level);
                output.Category.assign(message.logger_name.data(), message.logger_name.size());
                output.Text.assign(message.payload.data(), message.payload.size());
                Publish(std::move(output));
            }

            void flush_() override { }
        };

        std::shared_ptr<spdlog::logger> GetLogger(std::string_view category)
        {
            if (!State().Initialized)
            {
                Log::Initialize();
            }

            const std::string name(category);
            {
                std::lock_guard lock(State().Mutex);
                const auto iterator = State().Loggers.find(name);
                if (iterator != State().Loggers.end())
                {
                    return iterator->second;
                }
            }

            auto logger = std::make_shared<spdlog::logger>(name, State().SpdlogSinks.begin(),
                State().SpdlogSinks.end());
            logger->set_level(ToSpdlogLevel(State().Level));
            logger->flush_on(spdlog::level::warn);

            std::lock_guard lock(State().Mutex);
            const auto [iterator, inserted] = State().Loggers.emplace(name, std::move(logger));
            (void)inserted;
            return iterator->second;
        }
    }

    void AssertFailed(const char* expression, const char* message, const char* file, int line)
    {
        ASTER_LOG_CRITICAL("Assert", "Assertion failed: %s (%s) at %s:%d", expression,
            message != nullptr ? message : "", file, line);
#if defined(_WIN32)
        OutputDebugStringA("Aster assertion failed\n");
        if (IsDebuggerPresent())
        {
            __debugbreak();
        }
#endif
        std::abort();
    }

    void Log::Initialize()
    {
        std::lock_guard lock(State().Mutex);
        if (State().Initialized)
        {
            return;
        }

        auto bridge = std::make_shared<BridgeSink>();
        bridge->set_pattern("[%l][%n] %v");
        auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console->set_pattern("[%l][%n] %v");
#if defined(_WIN32)
        auto debugger = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        debugger->set_pattern("[%l][%n] %v");
        State().SpdlogSinks = { bridge, console, debugger };
#else
        State().SpdlogSinks = { bridge, console };
#endif
        State().Bridge = std::move(bridge);
        State().Initialized = true;
    }

    void Log::Shutdown()
    {
        std::lock_guard lock(State().Mutex);
        State().Loggers.clear();
        State().Sinks.clear();
        State().Messages.clear();
        State().SpdlogSinks.clear();
        State().Bridge.reset();
        State().Initialized = false;
    }

    void Log::SetLevel(LogLevel level)
    {
        std::lock_guard lock(State().Mutex);
        State().Level = level;
        for (const auto& [name, logger] : State().Loggers)
        {
            (void)name;
            logger->set_level(ToSpdlogLevel(level));
        }
    }

    LogLevel Log::GetLevel()
    {
        std::lock_guard lock(State().Mutex);
        return State().Level;
    }

    void Log::Write(LogLevel level, std::string_view category, const char* format, ...)
    {
        if (format == nullptr)
        {
            return;
        }

        char buffer[MaxMessageLength];
        va_list args;
        va_start(args, format);
        const int written = std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        if (written < 0)
        {
            buffer[0] = '\0';
        }
        buffer[sizeof(buffer) - 1] = '\0';

        const std::shared_ptr<spdlog::logger> logger = GetLogger(category);
        if (logger)
        {
            logger->log(ToSpdlogLevel(level), "{}", std::string_view(buffer));
        }
    }

    std::vector<LogMessage> Log::GetMessages()
    {
        std::lock_guard lock(State().Mutex);
        return { State().Messages.begin(), State().Messages.end() };
    }

    void Log::Clear()
    {
        std::lock_guard lock(State().Mutex);
        State().Messages.clear();
    }

    Log::SinkId Log::AddSink(Sink sink)
    {
        std::lock_guard lock(State().Mutex);
        const SinkId id = State().NextSinkId++;
        State().Sinks.emplace(id, std::move(sink));
        return id;
    }

    void Log::RemoveSink(SinkId id)
    {
        std::lock_guard lock(State().Mutex);
        State().Sinks.erase(id);
    }

    std::string_view Log::LevelToString(LogLevel level)
    {
        switch (level)
        {
            case LogLevel::Trace: return "TRACE";
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info: return "INFO";
            case LogLevel::Warning: return "WARN";
            case LogLevel::Error: return "ERROR";
            case LogLevel::Critical: return "CRIT";
            case LogLevel::Off: return "OFF";
        }
        return "UNKNOWN";
    }
}
