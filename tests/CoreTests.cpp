#include "TestFramework.h"

#include "Aster/Core/FileSystem.h"
#include "Aster/Core/UUID.h"
#include "Aster/Core/Log.h"
#include "Aster/Math/Math.h"

#include <filesystem>

using namespace Aster;

ASTER_TEST(Core, UuidRoundTrip)
{
    const UUID id = UUID::Generate();
    ASTER_CHECK(!id.IsNil());
    const std::string text = id.ToString();
    const std::optional<UUID> parsed = UUID::FromString(text);
    ASTER_CHECK(parsed.has_value());
    ASTER_CHECK_EQ(parsed->ToString(), text);
    ASTER_CHECK(*parsed == id);
}

ASTER_TEST(Core, UuidRejectsInvalidText)
{
    ASTER_CHECK(!UUID::FromString("not-a-uuid").has_value());
    ASTER_CHECK(!UUID::FromString("").has_value());
    ASTER_CHECK(!UUID::FromString("00000000-0000-0000-0000-00000000000").has_value());
}

ASTER_TEST(Core, FileSystemTextRoundTrip)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "aster_engine_test_file.txt";
    ASTER_CHECK(FileSystem::WriteTextFile(path, "hello aster\n"));
    const std::optional<std::string> contents = FileSystem::ReadTextFile(path);
    ASTER_CHECK(contents.has_value());
    ASTER_CHECK_EQ(*contents, std::string("hello aster\n"));
    std::filesystem::remove(path);
}

ASTER_TEST(Core, FileSystemPathHelpers)
{
    ASTER_CHECK_EQ(FileSystem::GetExtension("C:/assets/model.gltf"), std::string("gltf"));
    ASTER_CHECK_EQ(FileSystem::GetStem("C:/assets/model.gltf"), std::string("model"));
    ASTER_CHECK_EQ(FileSystem::GetFilename("C:/assets/model.gltf"), std::string("model.gltf"));
    ASTER_CHECK_EQ(FileSystem::Normalize("assets\\models\\cube.obj"), std::string("assets/models/cube.obj"));
}

ASTER_TEST(Core, LogRingBuffer)
{
    Log::Initialize();
    Log::Clear();
    Log::SetLevel(LogLevel::Trace);
    ASTER_LOG_INFO("Test", "value %d", 42);
    ASTER_LOG_WARN("Test", "warning %s", "message");
    const std::vector<LogMessage> messages = Log::GetMessages();
    ASTER_CHECK(messages.size() >= 2);
    ASTER_CHECK_EQ(messages[messages.size() - 2].Text, std::string("value 42"));
    ASTER_CHECK_EQ(messages.back().Level, LogLevel::Warning);
    Log::Shutdown();
}

ASTER_TEST(Core, LogSinkReceivesFormattedMessages)
{
    Log::Initialize();
    Log::SetLevel(LogLevel::Trace);
    std::vector<LogMessage> received;
    const Log::SinkId id = Log::AddSink([&](const LogMessage& message) { received.push_back(message); });
    ASTER_LOG_ERROR("Console", "sink %d", 7);
    Log::RemoveSink(id);

    ASTER_CHECK(!received.empty());
    ASTER_CHECK_EQ(received.back().Category, std::string("Console"));
    ASTER_CHECK_EQ(received.back().Text, std::string("sink 7"));
    ASTER_CHECK_EQ(received.back().Level, LogLevel::Error);
    Log::Shutdown();
}

ASTER_TEST(Math, TransformComposeAndDecompose)
{
    Transform transform;
    transform.Translation = glm::vec3(3.0f, -2.0f, 5.0f);
    transform.Rotation = glm::vec3(10.0f, 25.0f, 45.0f);
    transform.Scale = glm::vec3(2.0f, 0.5f, 1.5f);

    const glm::mat4 matrix = transform.GetMatrix();
    Transform decomposed;
    ASTER_CHECK(DecomposeTransform(matrix, decomposed));
    ASTER_CHECK_NEAR(decomposed.Translation.x, transform.Translation.x, 1e-4);
    ASTER_CHECK_NEAR(decomposed.Translation.y, transform.Translation.y, 1e-4);
    ASTER_CHECK_NEAR(decomposed.Translation.z, transform.Translation.z, 1e-4);
    ASTER_CHECK_NEAR(decomposed.Scale.x, transform.Scale.x, 1e-4);
    ASTER_CHECK_NEAR(decomposed.Scale.y, transform.Scale.y, 1e-4);
}

ASTER_TEST(Math, TransformRotatesPoint)
{
    Transform transform;
    transform.Rotation = glm::vec3(0.0f, 0.0f, 90.0f);
    const glm::vec3 point = transform.TransformPoint(glm::vec3(1.0f, 0.0f, 0.0f));
    ASTER_CHECK_NEAR(point.x, 0.0f, 1e-4);
    ASTER_CHECK_NEAR(point.y, 1.0f, 1e-4);
}

ASTER_TEST(Math, RayIntersections)
{
    const Aabb box{ glm::vec3(-1.0f), glm::vec3(1.0f) };
    const Ray ray{ glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f) };
    const std::optional<float> hit = IntersectRayAabb(ray, box, 100.0f);
    ASTER_CHECK(hit.has_value());
    ASTER_CHECK_NEAR(*hit, 4.0f, 1e-4);
    ASTER_CHECK(!IntersectRayAabb(Ray{ glm::vec3(5.0f, 5.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f) }, box,
        100.0f).has_value());

    const std::optional<float> sphere = IntersectRaySphere(ray, glm::vec3(0.0f), 1.0f, 100.0f);
    ASTER_CHECK(sphere.has_value());
    ASTER_CHECK_NEAR(*sphere, 4.0f, 1e-4);
}

ASTER_TEST(Math, AabbTransform)
{
    const Aabb box{ glm::vec3(-1.0f), glm::vec3(1.0f) };
    const glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f));
    const Aabb transformed = box.Transformed(transform);
    ASTER_CHECK_NEAR(transformed.Center().x, 10.0f, 1e-4);
    ASTER_CHECK_NEAR(transformed.Size().x, 2.0f, 1e-4);
}
