#pragma once

#include "Aster/Assets/AssetManager.h"
#include "Aster/Scene/Scene.h"

#include <string>

namespace Aster
{
    class Prefab
    {
    public:
        // Saves the entity subtree rooted at root as a standalone scene document.
        [[nodiscard]] static bool Save(const Scene& scene, Entity root, const std::string& path,
            std::string* error = nullptr);

        // Loads a prefab document and instantiates all of its entities under parent.
        [[nodiscard]] static bool Instantiate(const std::string& path, Scene& destination, Entity parent,
            AssetManager* assets = nullptr, std::string* error = nullptr);
    };
}
