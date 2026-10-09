#include <Aster/Scene/Scene.h>

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
    void Check(bool condition, std::string_view message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }
}

int main()
{
    Aster::Scene scene("Test Scene");
    Check(scene.GetName() == "Test Scene", "scene keeps its name");
    Check(scene.GetEntityCount() == 0, "new scene is empty");

    const Aster::Entity entity = scene.CreateEntity("Player");
    Check(static_cast<bool>(entity), "created entity has a valid ID");
    Check(scene.IsAlive(entity), "created entity is alive");
    Check(scene.GetEntityCount() == 1, "creation updates entity count");
    Check(scene.GetName(entity) != nullptr && *scene.GetName(entity) == "Player",
          "entity keeps its name");

    Aster::TransformComponent* transform = scene.GetTransform(entity);
    Check(transform != nullptr, "entity has a transform");
    Check(transform->Scale == Aster::Vector3{ 1.0F, 1.0F, 1.0F },
          "transform scale defaults to one");
    transform->Translation = { 2.0F, 3.0F, 4.0F };
    Check(scene.GetTransform(entity)->Translation == Aster::Vector3{ 2.0F, 3.0F, 4.0F },
          "transform can be updated");

    Check(scene.DestroyEntity(entity), "existing entity can be destroyed");
    Check(!scene.IsAlive(entity), "destroyed entity is no longer alive");
    Check(scene.GetTransform(entity) == nullptr, "destroyed entity has no transform");
    Check(!scene.DestroyEntity(entity), "destroying an unknown entity reports failure");
    Check(scene.GetEntityCount() == 0, "destruction updates entity count");

    std::cout << "All Aster Engine tests passed.\n";
    return EXIT_SUCCESS;
}