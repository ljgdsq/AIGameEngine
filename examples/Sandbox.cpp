#include <Aster/Scene/Scene.h>

#include <iostream>

int main()
{
    Aster::Scene scene("Aster Sandbox");
    const Aster::Entity camera = scene.CreateEntity("Camera");
    const Aster::Entity cube = scene.CreateEntity("Cube");
    scene.GetTransform(cube)->Translation = { 0.0F, 0.0F, -3.0F };

    std::cout << scene.GetName() << " contains " << scene.GetEntityCount()
              << " entities. Camera ID: " << camera.Id << '\n';
    return 0;
}