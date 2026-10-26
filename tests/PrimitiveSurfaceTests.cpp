#include "TestFramework.h"

#include "Aster/Assets/Primitives.h"
#include "Aster/Physics/PhysicsWorld.h"
#include "Aster/Project/Project.h"
#include "Aster/Scene/Components.h"
#include "Aster/Scene/Scene.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace Aster;

namespace
{
    struct VertexBounds
    {
        glm::vec3 Min{ std::numeric_limits<float>::max() };
        glm::vec3 Max{ -std::numeric_limits<float>::max() };
        size_t Count = 0;
        float MaxRadius = 0.0f;
    };

    VertexBounds ParseVertexBounds(const std::string& obj)
    {
        VertexBounds bounds;
        std::istringstream stream(obj);
        std::string line;
        while (std::getline(stream, line))
        {
            if (line.size() < 2 || line[0] != 'v' || line[1] != ' ')
            {
                continue;
            }
            std::istringstream values(line.substr(2));
            glm::vec3 vertex(0.0f);
            if (!(values >> vertex.x >> vertex.y >> vertex.z))
            {
                continue;
            }
            bounds.Min = glm::min(bounds.Min, vertex);
            bounds.Max = glm::max(bounds.Max, vertex);
            bounds.MaxRadius = std::max(bounds.MaxRadius, glm::length(vertex));
            ++bounds.Count;
        }
        return bounds;
    }

    std::filesystem::path MakeTempRoot(const char* tag)
    {
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return std::filesystem::temp_directory_path() /
            (std::string("aster_primitive_test_") + tag + "_" + std::to_string(stamp));
    }

    Entity AddBox(Scene& scene, const std::string& name, const glm::vec3& position, const glm::vec3& scale,
        const glm::vec3& halfExtents, bool dynamic)
    {
        const Entity entity = scene.CreateEntity(name);
        TransformComponent& transform = scene.GetRegistry().Get<TransformComponent>(entity);
        transform.Local.Translation = position;
        transform.Local.Scale = scale;
        RigidBodyComponent body;
        body.Type = dynamic ? BodyType::Dynamic : BodyType::Static;
        body.UseGravity = dynamic;
        scene.GetRegistry().Emplace<RigidBodyComponent>(entity, body);
        BoxColliderComponent collider;
        collider.HalfExtents = halfExtents;
        scene.GetRegistry().Emplace<BoxColliderComponent>(entity, collider);
        return entity;
    }

    Entity AddSphere(Scene& scene, const std::string& name, const glm::vec3& position, float visualScale,
        float colliderRadius, bool dynamic)
    {
        const Entity entity = scene.CreateEntity(name);
        TransformComponent& transform = scene.GetRegistry().Get<TransformComponent>(entity);
        transform.Local.Translation = position;
        transform.Local.Scale = glm::vec3(visualScale);
        RigidBodyComponent body;
        body.Type = dynamic ? BodyType::Dynamic : BodyType::Static;
        body.UseGravity = dynamic;
        body.Mass = 1.0f;
        scene.GetRegistry().Emplace<RigidBodyComponent>(entity, body);
        SphereColliderComponent collider;
        collider.Radius = colliderRadius;
        scene.GetRegistry().Emplace<SphereColliderComponent>(entity, collider);
        return entity;
    }

    float SimulateUntilRest(Scene& scene, Entity entity, int steps)
    {
        PhysicsWorld physics;
        physics.Rebuild(scene);
        for (int step = 0; step < steps; ++step)
        {
            physics.Step(1.0f / 60.0f);
            physics.SyncToScene(scene);
        }
        return scene.GetRegistry().Get<TransformComponent>(entity).Local.Translation.y;
    }
}

ASTER_TEST(Primitives, UnitSizedCubeSphereAndPlane)
{
    const VertexBounds cube = ParseVertexBounds(Primitives::CubeObj());
    ASTER_CHECK_EQ(cube.Count, 24U);
    ASTER_CHECK_NEAR(cube.Min.x, -0.5f, 0.0001f);
    ASTER_CHECK_NEAR(cube.Min.y, -0.5f, 0.0001f);
    ASTER_CHECK_NEAR(cube.Min.z, -0.5f, 0.0001f);
    ASTER_CHECK_NEAR(cube.Max.x, 0.5f, 0.0001f);
    ASTER_CHECK_NEAR(cube.Max.y, 0.5f, 0.0001f);
    ASTER_CHECK_NEAR(cube.Max.z, 0.5f, 0.0001f);
    ASTER_CHECK_NEAR(cube.MaxRadius, 0.8660254f, 0.001f);

    const VertexBounds sphere = ParseVertexBounds(Primitives::SphereObj(8, 12));
    ASTER_CHECK(sphere.Count > 0);
    ASTER_CHECK_NEAR(sphere.Min.x, -0.5f, 0.001f);
    ASTER_CHECK_NEAR(sphere.Min.y, -0.5f, 0.001f);
    ASTER_CHECK_NEAR(sphere.Min.z, -0.5f, 0.001f);
    ASTER_CHECK_NEAR(sphere.Max.x, 0.5f, 0.001f);
    ASTER_CHECK_NEAR(sphere.Max.y, 0.5f, 0.001f);
    ASTER_CHECK_NEAR(sphere.Max.z, 0.5f, 0.001f);
    ASTER_CHECK_NEAR(sphere.MaxRadius, 0.5f, 0.001f);

    const VertexBounds plane = ParseVertexBounds(Primitives::PlaneObj());
    ASTER_CHECK_EQ(plane.Count, 4U);
    ASTER_CHECK_NEAR(plane.Min.y, 0.0f, 0.0001f);
    ASTER_CHECK_NEAR(plane.Max.y, 0.0f, 0.0001f);
    ASTER_CHECK_NEAR(plane.Min.x, -0.5f, 0.0001f);
    ASTER_CHECK_NEAR(plane.Max.x, 0.5f, 0.0001f);
    ASTER_CHECK_NEAR(plane.Min.z, -0.5f, 0.0001f);
    ASTER_CHECK_NEAR(plane.Max.z, 0.5f, 0.0001f);
}

ASTER_TEST(Primitives, WriteDefaultPrimitivesProducesUnitMeshes)
{
    const std::filesystem::path root = MakeTempRoot("write");
    std::filesystem::remove_all(root);
    std::string error;
    ASTER_CHECK(Primitives::WriteDefaultPrimitives(root, &error));
    ASTER_CHECK(std::filesystem::exists(root / "cube.obj"));
    ASTER_CHECK(std::filesystem::exists(root / "sphere.obj"));
    ASTER_CHECK(std::filesystem::exists(root / "plane.obj"));

    {
        std::ifstream cube(root / "cube.obj");
        std::stringstream contents;
        contents << cube.rdbuf();
        const VertexBounds bounds = ParseVertexBounds(contents.str());
        ASTER_CHECK_NEAR(bounds.Min.y, -0.5f, 0.0001f);
        ASTER_CHECK_NEAR(bounds.Max.y, 0.5f, 0.0001f);
    }

    std::filesystem::remove_all(root);
}

ASTER_TEST(Primitives, MarbleRestsOnPlatformSurface)
{
    Scene scene("Surface Test");
    AddBox(scene, "Start Pad", glm::vec3(0.0f, -0.25f, 0.0f), glm::vec3(5.0f, 0.5f, 5.0f),
        glm::vec3(0.5f), false);
    const Entity marble = AddSphere(scene, "Marble", glm::vec3(0.0f, 2.0f, 0.0f), 0.8f, 0.5f, true);

    const float center = SimulateUntilRest(scene, marble, 240);
    // Visual scale 0.8 with a unit sphere gives radius 0.4; the collider radius 0.5 * 0.8 is also
    // 0.4, so the surface and the sphere agree and the center rests one radius above the pad.
    ASTER_CHECK_NEAR(center, 0.4f, 0.02f);
    ASTER_CHECK_NEAR(center - 0.4f, 0.0f, 0.02f);
}

ASTER_TEST(Primitives, MarbleDemoSettlesOnStartPad)
{
    const std::filesystem::path root = MakeTempRoot("demo");
    std::filesystem::remove_all(root);
    Project project;
    std::string error;
    ASTER_CHECK(project.CreateGameDemo(root, "Marble Surface", &error));

    Scene& scene = project.GetScene();
    const Entity marble = scene.FindByName("Marble");
    ASTER_CHECK(marble != NullEntity);
    if (marble == NullEntity)
    {
        std::filesystem::remove_all(root);
        return;
    }

    const float center = SimulateUntilRest(scene, marble, 300);
    ASTER_CHECK_NEAR(center, 0.4f, 0.03f);
    const TransformComponent& transform = scene.GetRegistry().Get<TransformComponent>(marble);
    ASTER_CHECK_NEAR(transform.Local.Scale.x, 0.8f, 0.0001f);
    ASTER_CHECK_NEAR(center - 0.4f, 0.0f, 0.03f);

    std::filesystem::remove_all(root);
}
