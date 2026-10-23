#include "TestFramework.h"

#include "Aster/ECS/Registry.h"

using namespace Aster;

namespace
{
    struct Health
    {
        int Value = 100;
    };

    struct Position
    {
        float X = 0.0f;
        float Y = 0.0f;
    };
}

ASTER_TEST(Ecs, CreateDestroyGenerations)
{
    Registry registry;
    const Entity first = registry.Create();
    ASTER_CHECK(registry.IsAlive(first));
    ASTER_CHECK(registry.Destroy(first));
    ASTER_CHECK(!registry.IsAlive(first));

    const Entity second = registry.Create();
    ASTER_CHECK(registry.IsAlive(second));
    ASTER_CHECK_EQ(second.Index(), first.Index());
    ASTER_CHECK(second.Generation() != first.Generation());
    ASTER_CHECK(!registry.IsAlive(first));
}

ASTER_TEST(Ecs, Components)
{
    Registry registry;
    Entity entity = registry.Create();
    registry.Emplace<Health>(entity, Health{ 75 });
    registry.Emplace<Position>(entity, Position{ 1.0f, 2.0f });

    ASTER_CHECK(registry.Has<Health>(entity));
    ASTER_CHECK_EQ(registry.Get<Health>(entity).Value, 75);
    ASTER_CHECK_NEAR(registry.Get<Position>(entity).Y, 2.0f, 1e-6);

    registry.Remove<Health>(entity);
    ASTER_CHECK(!registry.Has<Health>(entity));
    ASTER_CHECK(registry.Has<Position>(entity));

    registry.Destroy(entity);
    ASTER_CHECK(!registry.Has<Position>(entity));
}

ASTER_TEST(Ecs, EachVisitsMatchingEntities)
{
    Registry registry;
    const Entity first = registry.Create();
    const Entity second = registry.Create();
    const Entity third = registry.Create();
    registry.Emplace<Health>(first, Health{ 1 });
    registry.Emplace<Health>(second, Health{ 2 });
    registry.Emplace<Position>(third, Position{ 3.0f, 4.0f });

    int visited = 0;
    registry.Each<Health>([&](Entity, Health& health) {
        ++visited;
        health.Value += 10;
    });

    ASTER_CHECK_EQ(visited, 2);
    ASTER_CHECK_EQ(registry.Get<Health>(first).Value, 11);
    ASTER_CHECK_EQ(registry.Get<Health>(second).Value, 12);
}

ASTER_TEST(Ecs, ClearRemovesEverything)
{
    Registry registry;
    for (int index = 0; index < 8; ++index)
    {
        Entity entity = registry.Create();
        registry.Emplace<Health>(entity);
    }
    ASTER_CHECK_EQ(registry.GetEntityCount(), 8);
    registry.Clear();
    ASTER_CHECK_EQ(registry.GetEntityCount(), 0);
    ASTER_CHECK(registry.GetAliveEntities().empty());
    const Entity fresh = registry.Create();
    ASTER_CHECK(registry.IsAlive(fresh));
}

ASTER_TEST(Ecs, DeadEntitiesAreNotVisited)
{
    Registry registry;
    Entity entity = registry.Create();
    registry.Emplace<Health>(entity);
    registry.Destroy(entity);
    int visited = 0;
    registry.Each<Health>([&](Entity, Health&) { ++visited; });
    ASTER_CHECK_EQ(visited, 0);
}
