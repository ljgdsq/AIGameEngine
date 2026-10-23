#include "TestFramework.h"

#include "Aster/Rendering/DebugDraw.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

using namespace Aster;

ASTER_TEST(DebugDraw, BoxProducesTwelveWorldSpaceEdges)
{
    std::vector<DebugVertex> vertices;
    const glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.0f));
    DebugDraw::AppendBox(vertices, transform, glm::vec3(0.0f), glm::vec3(1.0f, 2.0f, 3.0f),
        glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    ASTER_CHECK_EQ(vertices.size(), static_cast<size_t>(24));

    float minX = 1e9f;
    float maxX = -1e9f;
    for (const DebugVertex& vertex : vertices)
    {
        minX = std::min(minX, vertex.Position.x);
        maxX = std::max(maxX, vertex.Position.x);
        ASTER_CHECK_NEAR(vertex.Color.g, 1.0f, 1e-5);
    }
    ASTER_CHECK_NEAR(minX, 1.0f, 1e-4);
    ASTER_CHECK_NEAR(maxX, 3.0f, 1e-4);
}

ASTER_TEST(DebugDraw, SphereProducesThreeRings)
{
    std::vector<DebugVertex> vertices;
    DebugDraw::AppendSphere(vertices, glm::mat4(1.0f), glm::vec3(0.0f), 2.0f,
        glm::vec4(1.0f), 8);
    ASTER_CHECK_EQ(vertices.size(), static_cast<size_t>(8 * 3 * 2));
    for (const DebugVertex& vertex : vertices)
    {
        ASTER_CHECK_NEAR(glm::length(vertex.Position), 2.0f, 1e-3);
    }
}

ASTER_TEST(DebugDraw, SceneCollidersUseEntityTransforms)
{
    Scene scene;
    const Entity box = scene.CreateEntity("Box");
    scene.GetRegistry().Get<TransformComponent>(box).Local.Translation = glm::vec3(4.0f, 0.0f, 0.0f);
    scene.GetRegistry().Emplace<BoxColliderComponent>(box, BoxColliderComponent{});

    const Entity sphere = scene.CreateEntity("Sphere");
    scene.GetRegistry().Get<TransformComponent>(sphere).Local.Translation = glm::vec3(0.0f, 3.0f, 0.0f);
    SphereColliderComponent collider;
    collider.Radius = 1.0f;
    scene.GetRegistry().Emplace<SphereColliderComponent>(sphere, collider);

    std::vector<DebugVertex> vertices;
    DebugDraw::AppendSceneColliders(scene, vertices);
    ASTER_CHECK_EQ(vertices.size(), static_cast<size_t>(24 + 24 * 3 * 2));
}
