#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace AsterTest
{
    class TestFailure : public std::runtime_error
    {
    public:
        explicit TestFailure(const std::string& message) : std::runtime_error(message) { }
    };

    struct TestCase
    {
        std::string Name;
        std::function<void()> Function;
    };

    inline std::vector<TestCase>& GetTests()
    {
        static std::vector<TestCase> tests;
        return tests;
    }

    inline bool RegisterTest(const char* name, std::function<void()> function)
    {
        GetTests().push_back(TestCase{ name, std::move(function) });
        return true;
    }

    inline int RunAll(int argc, char** argv)
    {
        const bool listOnly = argc > 1 && std::string(argv[1]) == "--list";
        if (listOnly)
        {
            for (const TestCase& test : GetTests())
            {
                std::printf("%s\n", test.Name.c_str());
            }
            return 0;
        }

        size_t passed = 0;
        size_t failed = 0;
        for (const TestCase& test : GetTests())
        {
            try
            {
                test.Function();
                ++passed;
                std::printf("[PASS] %s\n", test.Name.c_str());
            }
            catch (const std::exception& exception)
            {
                ++failed;
                std::printf("[FAIL] %s: %s\n", test.Name.c_str(), exception.what());
            }
            catch (...)
            {
                ++failed;
                std::printf("[FAIL] %s: unknown exception\n", test.Name.c_str());
            }
        }

        std::printf("\n%zu passed, %zu failed, %zu total\n", passed, failed, passed + failed);
        return failed == 0 ? 0 : 1;
    }

    inline void Fail(const char* file, int line, const std::string& message)
    {
        std::ostringstream stream;
        stream << file << ':' << line << ": " << message;
        throw TestFailure(stream.str());
    }
}

#define ASTER_TEST(suite, name) \
    static void suite##_##name##_TestFunction(); \
    [[maybe_unused]] static const bool suite##_##name##_Registered = \
        AsterTest::RegisterTest(#suite "." #name, suite##_##name##_TestFunction); \
    static void suite##_##name##_TestFunction()

#define ASTER_CHECK(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            AsterTest::Fail(__FILE__, __LINE__, "check failed: " #condition); \
        } \
    } while (false)

#define ASTER_CHECK_EQ(actual, expected) \
    do \
    { \
        const auto& asterActualValue = (actual); \
        const auto& asterExpectedValue = (expected); \
        if (!(asterActualValue == asterExpectedValue)) \
        { \
            std::ostringstream asterStream; \
            asterStream << "expected " #actual " == " #expected; \
            AsterTest::Fail(__FILE__, __LINE__, asterStream.str()); \
        } \
    } while (false)

#define ASTER_CHECK_NEAR(actual, expected, tolerance) \
    do \
    { \
        const double asterActualValue = static_cast<double>(actual); \
        const double asterExpectedValue = static_cast<double>(expected); \
        if (std::abs(asterActualValue - asterExpectedValue) > static_cast<double>(tolerance)) \
        { \
            std::ostringstream asterStream; \
            asterStream << "expected " #actual " near " #expected << " (actual " << asterActualValue \
                        << ", expected " << asterExpectedValue << ')'; \
            AsterTest::Fail(__FILE__, __LINE__, asterStream.str()); \
        } \
    } while (false)

#define ASTER_CHECK_THROWS(expression) \
    do \
    { \
        bool asterThrew = false; \
        try \
        { \
            (void)(expression); \
        } \
        catch (...) \
        { \
            asterThrew = true; \
        } \
        if (!asterThrew) \
        { \
            AsterTest::Fail(__FILE__, __LINE__, "expected exception: " #expression); \
        } \
    } while (false)
