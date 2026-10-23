#include "TestFramework.h"

#include "Aster/Physics/PhysicsWorld.h"
#include "Aster/Runtime/MarbleGame.h"
#include "Aster/Scene/Components.h"
#include "Aster/Scene/Scene.h"

#include <cmath>

using namespace Aster;

namespace
{
    Entity MakeSphere(Scene& scene, const std::string& name, const glm::vec3& position, float radius,
        bool dynamic)
    {
        const Entity entity = scene.CreateEntity(name);
        TransformComponent& transform = scene.GetRegistry().Get<TransformComponent>(entity);
        transform.Local.Translation = position;
        transform.Local.Scale = glm::vec3(radius * 2.0f);
        RigidBodyComponent body;
        body.Type = dynamic ? BodyType::Dynamic : BodyType::Static;
        body.UseGravity = dynamic;
        scene.GetRegistry().Emplace<RigidBodyComponent>(entity, body);
        SphereColliderComponent collider;
        collider.Radius = radius;
        scene.GetRegistry().Emplace<SphereColliderComponent>(entity, collider);
        return entity;
    }
}

ASTER_TEST(MarbleGame, CollectsCoinAndReportsHudState)
{
    Scene scene("Marble Test");
    const Entity player = MakeSphere(scene, "Marble", glm::vec3(0.0f, 1.0f, 0.0f), 0.4f, true);
    const Entity camera = scene.CreateEntity("Main Camera");
    CameraComponent cameraComponent;
    cameraComponent.Primary = true;
    scene.GetRegistry().Emplace<CameraComponent>(camera, cameraComponent);
    const Entity goal = scene.CreateEntity("Goal");
    scene.GetRegistry().Get<TransformComponent>(goal).Local.Translation = glm::vec3(0.0f, 1.0f, -8.0f);
    const Entity coin = scene.CreateEntity("Coin 01");
    scene.GetRegistry().Get<TransformComponent>(coin).Local.Translation = glm::vec3(0.0f, 1.0f, 0.0f);

    PhysicsWorld physics;
    physics.Rebuild(scene);
    MarbleGame game;
    ASTER_CHECK(game.Initialize(scene, physics));
    ASTER_CHECK(game.IsActive());
    ASTER_CHECK_EQ(game.GetTotalCoins(), 1);
    ASTER_CHECK_EQ(game.GetScore(), 0);

    game.Update(0.016f, MarbleInput{});
    ASTER_CHECK_EQ(game.GetScore(), 1);

    physics.SetLinearVelocity(player, glm::vec3(1.5f, 0.0f, -2.0f));
    const glm::vec3 velocity = physics.GetLinearVelocity(player);
    ASTER_CHECK(std::abs(velocity.x - 1.5f) < 0.001f);
    ASTER_CHECK(std::abs(velocity.z + 2.0f) < 0.001f);

    game.Reset();
    ASTER_CHECK_EQ(game.GetScore(), 0);
    ASTER_CHECK(!game.IsComplete());
    ASTER_CHECK(std::abs(game.GetElapsedTime()) < 0.001f);
}

ASTER_TEST(MarbleGame, CompletesAtGoalAndCanRestart)
{
    Scene scene("Marble Goal Test");
    const Entity player = MakeSphere(scene, "Marble", glm::vec3(0.0f, 1.0f, 0.0f), 0.4f, true);
    const Entity camera = scene.CreateEntity("Main Camera");
    CameraComponent cameraComponent;
    cameraComponent.Primary = true;
    scene.GetRegistry().Emplace<CameraComponent>(camera, cameraComponent);
    const Entity goal = scene.CreateEntity("Goal");
    scene.GetRegistry().Get<TransformComponent>(goal).Local.Translation = glm::vec3(0.0f, 1.0f, 0.0f);

    PhysicsWorld physics;
    physics.Rebuild(scene);
    MarbleGame game;
    ASTER_CHECK(game.Initialize(scene, physics));
    game.Update(0.016f, MarbleInput{});
    ASTER_CHECK(game.IsComplete());
    ASTER_CHECK(!game.GetStatusMessage().empty());

    game.Reset();
    ASTER_CHECK(!game.IsComplete());
    ASTER_CHECK(scene.IsAlive(player));
}

ASTER_TEST(MarbleGame, FallingResetsPlayerToStart)
{
    Scene scene("Marble Fall Test");
    const Entity player = MakeSphere(scene, "Marble", glm::vec3(0.0f, 1.0f, 0.0f), 0.4f, true);
    const Entity camera = scene.CreateEntity("Main Camera");
    CameraComponent cameraComponent;
    cameraComponent.Primary = true;
    scene.GetRegistry().Emplace<CameraComponent>(camera, cameraComponent);
    const Entity goal = scene.CreateEntity("Goal");
    scene.GetRegistry().Get<TransformComponent>(goal).Local.Translation = glm::vec3(0.0f, 1.0f, -8.0f);

    PhysicsWorld physics;
    physics.Rebuild(scene);
    MarbleGame game;
    ASTER_CHECK(game.Initialize(scene, physics));

    scene.GetRegistry().Get<TransformComponent>(player).Local.Translation = glm::vec3(0.0f, -20.0f, 0.0f);
    physics.Rebuild(scene);
    game.Update(0.016f, MarbleInput{});
    const glm::vec3 position = game.GetPlayerPosition();
    ASTER_CHECK(std::abs(position.y - 1.0f) < 0.25f);
}

ASTER_TEST(MarbleGame, RejectsSceneWithoutGoal)
{
    Scene scene("Not A Marble Scene");
    MakeSphere(scene, "Marble", glm::vec3(0.0f), 0.4f, true);
    PhysicsWorld physics;
    physics.Rebuild(scene);
    MarbleGame game;
    ASTER_CHECK(!game.Initialize(scene, physics));
    ASTER_CHECK(!game.IsActive());
}
