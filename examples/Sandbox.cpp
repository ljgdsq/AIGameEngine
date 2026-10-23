#include "Aster/Core/Log.h"
#include "Aster/Physics/PhysicsWorld.h"
#include "Aster/Scene/Scene.h"

#include <cmath>

using namespace Aster;

namespace
{
    Entity AddBody(Scene& scene, const char* name, BodyType type, const glm::vec3& position)
    {
        const Entity entity = scene.CreateEntity(name);
        scene.GetRegistry().Get<TransformComponent>(entity).Local.Translation = position;
        RigidBodyComponent body;
        body.Type = type;
        body.UseGravity = type == BodyType::Dynamic;
        scene.GetRegistry().Emplace<RigidBodyComponent>(entity, body);
        return entity;
    }
}

int main()
{
    Log::Initialize();
    Scene scene("Sandbox");

    const Entity ground = AddBody(scene, "Ground", BodyType::Static, glm::vec3(0.0f));
    BoxColliderComponent groundCollider;
    groundCollider.HalfExtents = glm::vec3(10.0f, 0.5f, 10.0f);
    scene.GetRegistry().Emplace<BoxColliderComponent>(ground, groundCollider);

    const Entity box = AddBody(scene, "Box", BodyType::Dynamic, glm::vec3(0.0f, 5.0f, 0.0f));
    BoxColliderComponent boxCollider;
    boxCollider.HalfExtents = glm::vec3(0.5f);
    scene.GetRegistry().Emplace<BoxColliderComponent>(box, boxCollider);

    PhysicsWorld physics;
    physics.Rebuild(scene);
    for (int step = 0; step < 180; ++step)
    {
        physics.Step(1.0f / 60.0f);
        physics.SyncToScene(scene);
    }

    const float height = scene.GetRegistry().Get<TransformComponent>(box).Local.Translation.y;
    ASTER_LOG_INFO("Sandbox", "Box settled at y=%.3f after 180 fixed steps", height);
    Log::Shutdown();
    return std::isfinite(height) && height > 0.4f && height < 5.0f ? 0 : 1;
}
