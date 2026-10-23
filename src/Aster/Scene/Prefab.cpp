#include "Aster/Scene/Prefab.h"

#include "Aster/Scene/SceneSerializer.h"

namespace Aster
{
    bool Prefab::Save(const Scene& scene, Entity root, const std::string& path, std::string* error)
    {
        if (!scene.IsAlive(root))
        {
            if (error != nullptr)
            {
                *error = "Prefab root entity is not alive";
            }
            return false;
        }

        Scene prefabScene("Prefab");
        (void)prefabScene.InstantiateEntity(scene, root);
        prefabScene.GetSettings().Name = "Prefab";
        return SceneSerializer::Save(prefabScene, path, error);
    }

    bool Prefab::Instantiate(const std::string& path, Scene& destination, Entity parent, AssetManager* assets,
        std::string* error)
    {
        Scene prefabScene("Prefab");
        if (!SceneSerializer::Load(prefabScene, path, assets, error))
        {
            return false;
        }
        destination.InstantiateScene(prefabScene, parent);
        return true;
    }
}
