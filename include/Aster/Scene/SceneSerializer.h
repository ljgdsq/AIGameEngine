#pragma once

#include "Aster/Assets/AssetManager.h"
#include "Aster/Scene/Scene.h"

#include <string>
#include <string_view>

namespace Aster
{
    class SceneSerializer
    {
    public:
        [[nodiscard]] static bool Save(const Scene& scene, const std::string& path, std::string* error = nullptr);
        [[nodiscard]] static std::string Serialize(const Scene& scene);
        [[nodiscard]] static bool Load(Scene& scene, const std::string& path, AssetManager* assets = nullptr,
            std::string* error = nullptr);
        [[nodiscard]] static bool LoadFromString(Scene& scene, std::string_view yaml, AssetManager* assets = nullptr,
            std::string* error = nullptr);
    };
}
