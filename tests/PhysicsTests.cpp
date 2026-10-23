#include "TestFramework.h"

#include "Aster/Physics/PhysicsWorld.h"

using namespace Aster;

namespace
{
    Entity CreateBox(Scene& scene, const std::string& name, const glm::vec3& position, const glm::vec3& halfExtents,
        BodyType type)
    {
        const Entity entity = scene.CreateEntity(name);
        scene.GetRegistry().Get<TransformComponent>(entity).Local.Translation = position;
        RigidBodyComponent body;
        body.Type = type;
        scene.GetRegistry().Emplace<RigidBodyComponent>(entity, body);
        BoxColliderComponent collider;
        collider.HalfExtents = halfExtents;
        scene.GetRegistry().Emplace<BoxColliderComponent>(entity, collider);
        return entity;
    }

    Entity CreateSphere(Scene& scene, const std::string& name, const glm::vec3& position, float radius,
        BodyType type)
    {
        const Entity entity = scene.CreateEntity(name);
        scene.GetRegistry().Get<TransformComponent>(entity).Local.Translation = position;
        RigidBodyComponent body;
        body.Type = type;
        scene.GetRegistry().Emplace<RigidBodyComponent>(entity, body);
        SphereColliderComponent collider;
        collider.Radius = radius;
        scene.GetRegistry().Emplace<SphereColliderComponent>(entity, collider);
        return entity;
    }
}

ASTER_TEST(Physics, SphereFallsAndRestsOnGround)
{
    Scene scene("Physics");
    CreateBox(scene, "Ground", glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(10.0f, 0.5f, 10.0f), BodyType::Static);
    const Entity sphere = CreateSphere(scene, "Sphere", glm::vec3(0.0f, 3.0f, 0.0f), 0.5f, BodyType::Dynamic);

    PhysicsWorld physics;
    physics.Rebuild(scene);
    ASTER_CHECK_EQ(physics.GetBodyCount(), 2);
    for (int step = 0; step < 180; ++step)
    {
        physics.Step(1.0f / 60.0f);
    }
    physics.SyncToScene(scene);

    const Transform transform = scene.GetWorldTransform(sphere);
    ASTER_CHECK(transform.Translation.y > 0.4f);
    ASTER_CHECK(transform.Translation.y < 0.8f);
    ASTER_CHECK(physics.IsGrounded(sphere));
    ASTER_CHECK(!physics.GetCollisionEvents().empty());
}

ASTER_TEST(Physics, TwoSpheresSeparate)
{
    Scene scene("Physics");
    const Entity first = CreateSphere(scene, "A", glm::vec3(-0.6f, 0.0f, 0.0f), 0.5f, BodyType::Dynamic);
    const Entity second = CreateSphere(scene, "B", glm::vec3(0.6f, 0.0f, 0.0f), 0.5f, BodyType::Dynamic);
    scene.GetRegistry().Get<RigidBodyComponent>(first).LinearVelocity = glm::vec3(1.0f, 0.0f, 0.0f);
    scene.GetRegistry().Get<RigidBodyComponent>(second).LinearVelocity = glm::vec3(-1.0f, 0.0f, 0.0f);

    PhysicsWorld physics;
    physics.Rebuild(scene);
    for (int step = 0; step < 30; ++step)
    {
        physics.Step(1.0f / 60.0f);
    }
    physics.SyncToScene(scene);

    const float firstX = scene.GetWorldTransform(first).Translation.x;
    const float secondX = scene.GetWorldTransform(second).Translation.x;
    ASTER_CHECK(secondX > firstX);
    ASTER_CHECK(secondX - firstX >= 0.9f);
}

ASTER_TEST(Physics, KinematicBodyIgnoresGravity)
{
    Scene scene("Physics");
    const Entity kinematic = CreateBox(scene, "Kinematic", glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(0.5f),
        BodyType::Kinematic);
    scene.GetRegistry().Get<RigidBodyComponent>(kinematic).LinearVelocity = glm::vec3(1.0f, 0.0f, 0.0f);

    PhysicsWorld physics;
    physics.Rebuild(scene);
    for (int step = 0; step < 60; ++step)
    {
        physics.Step(1.0f / 60.0f);
    }
    physics.SyncToScene(scene);

    const Transform transform = scene.GetWorldTransform(kinematic);
    ASTER_CHECK_NEAR(transform.Translation.y, 5.0f, 1e-4);
    ASTER_CHECK(transform.Translation.x > 0.5f);
}

ASTER_TEST(Physics, RaycastHitsNearestBody)
{
    Scene scene("Physics");
    CreateSphere(scene, "Near", glm::vec3(0.0f, 0.0f, -3.0f), 0.5f, BodyType::Static);
    CreateSphere(scene, "Far", glm::vec3(0.0f, 0.0f, -8.0f), 0.5f, BodyType::Static);

    PhysicsWorld physics;
    physics.Rebuild(scene);
    const std::optional<RaycastHit> hit = physics.Raycast(Ray{ glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f) },
        100.0f);
    ASTER_CHECK(hit.has_value());
    ASTER_CHECK_NEAR(hit->Distance, 2.5f, 1e-3);
    const Entity nearEntity = scene.FindByName("Near");
    ASTER_CHECK_EQ(hit->HitEntity, nearEntity);
}

ASTER_TEST(Physics, TriggerProducesEventWithoutResponse)
{
    Scene scene("Physics");
    const Entity trigger = CreateBox(scene, "Trigger", glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f),
        BodyType::Static);
    scene.GetRegistry().Get<BoxColliderComponent>(trigger).IsTrigger = true;
    CreateSphere(scene, "Sphere", glm::vec3(0.0f, 0.0f, 0.0f), 0.5f, BodyType::Dynamic);

    PhysicsWorld physics;
    physics.Rebuild(scene);
    physics.Step(1.0f / 60.0f);
    ASTER_CHECK(!physics.GetCollisionEvents().empty());
    bool foundTrigger = false;
    for (const CollisionEvent& event : physics.GetCollisionEvents())
    {
        foundTrigger = foundTrigger || event.Trigger;
    }
    ASTER_CHECK(foundTrigger);
}
