#include "TestFramework.h"

#include "Aster/Core/Log.h"

#include <string>
#include <vector>

using namespace Aster;

ASTER_TEST(Log, LevelsMacrosAndMessageContents)
{
    Log::Initialize();
    Log::SetLevel(LogLevel::Trace);
    Log::Clear();

    ASTER_LOG_TRACE("Test", "trace message");
    ASTER_LOG_DEBUG("Test", "debug message");
    ASTER_LOG_INFO("Test", "info message");
    ASTER_LOG_WARN("Test", "warn message");
    ASTER_LOG_ERROR("Test", "error message");

    const std::vector<LogMessage> messages = Log::GetMessages();
#if ASTER_ENABLE_DEBUG_LOGS
    ASTER_CHECK_EQ(messages.size(), 5U);
    ASTER_CHECK(messages[0].Level == LogLevel::Trace);
    ASTER_CHECK(messages[1].Level == LogLevel::Debug);
    ASTER_CHECK(messages[2].Level == LogLevel::Info);
    ASTER_CHECK(messages[3].Level == LogLevel::Warning);
    ASTER_CHECK(messages[4].Level == LogLevel::Error);
    ASTER_CHECK_EQ(messages[2].Category, std::string("Test"));
    ASTER_CHECK_EQ(messages[2].Text, std::string("info message"));
#else
    // Trace/Debug statements are compiled out entirely in release-style builds.
    ASTER_CHECK_EQ(messages.size(), 3U);
    ASTER_CHECK(messages[0].Level == LogLevel::Info);
    ASTER_CHECK(messages[1].Level == LogLevel::Warning);
    ASTER_CHECK(messages[2].Level == LogLevel::Error);
    ASTER_CHECK_EQ(messages[0].Category, std::string("Test"));
    ASTER_CHECK_EQ(messages[0].Text, std::string("info message"));
#endif

    // Debug/Trace statements must compile to a no-op in builds where ASTER_ENABLE_DEBUG_LOGS is 0,
    // which also means the arguments are not evaluated.
    int evaluated = 0;
    ASTER_LOG_DEBUG("Test", "%d", ++evaluated);
#if ASTER_ENABLE_DEBUG_LOGS
    ASTER_CHECK_EQ(evaluated, 1);
#else
    ASTER_CHECK_EQ(evaluated, 0);
#endif

    Log::Clear();
    ASTER_CHECK(Log::GetMessages().empty());
    Log::SetLevel(LogLevel::Info);
}

ASTER_TEST(Log, RuntimeLevelFiltersSinksAndHistory)
{
    Log::Initialize();
    Log::SetLevel(LogLevel::Warning);
    Log::Clear();

    std::vector<LogMessage> received;
    const Log::SinkId sink = Log::AddSink([&received](const LogMessage& message)
    {
        received.push_back(message);
    });
    ASTER_CHECK(sink != 0);

    ASTER_LOG_INFO("Filter", "hidden info");
    ASTER_LOG_WARN("Filter", "visible warning");
    ASTER_LOG_ERROR("Filter", "visible error");

    ASTER_CHECK_EQ(received.size(), 2U);
    ASTER_CHECK(received[0].Level == LogLevel::Warning);
    ASTER_CHECK(received[1].Level == LogLevel::Error);
    ASTER_CHECK_EQ(Log::GetMessages().size(), 2U);

    Log::RemoveSink(sink);
    ASTER_LOG_ERROR("Filter", "after removal");
    ASTER_CHECK_EQ(received.size(), 2U);

    Log::SetLevel(LogLevel::Off);
    const size_t before = Log::GetMessages().size();
    ASTER_LOG_ERROR("Filter", "suppressed by Off");
    ASTER_CHECK_EQ(Log::GetMessages().size(), before);
    ASTER_CHECK(Log::GetLevel() == LogLevel::Off);

    Log::SetLevel(LogLevel::Debug);
    ASTER_CHECK(Log::GetLevel() == LogLevel::Debug);
}

ASTER_TEST(Log, LevelNamesAreStable)
{
    ASTER_CHECK_EQ(std::string(Log::LevelToString(LogLevel::Trace)), std::string("TRACE"));
    ASTER_CHECK_EQ(std::string(Log::LevelToString(LogLevel::Debug)), std::string("DEBUG"));
    ASTER_CHECK_EQ(std::string(Log::LevelToString(LogLevel::Info)), std::string("INFO"));
    ASTER_CHECK_EQ(std::string(Log::LevelToString(LogLevel::Warning)), std::string("WARN"));
    ASTER_CHECK_EQ(std::string(Log::LevelToString(LogLevel::Error)), std::string("ERROR"));
    ASTER_CHECK_EQ(std::string(Log::LevelToString(LogLevel::Critical)), std::string("CRIT"));
}
