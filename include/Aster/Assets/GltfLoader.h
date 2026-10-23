#pragma once

#include "Aster/Assets/Asset.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace Aster
{
    struct GltfLoadResult
    {
        std::shared_ptr<ModelAsset> Model;
        std::unordered_map<std::string, std::shared_ptr<MeshAsset>> Meshes;
        std::unordered_map<std::string, std::shared_ptr<Texture>> Textures;
        std::string Error;
    };

    class GltfLoader
    {
    public:
        // path is the file to read; keyPath is the stable project-relative asset key stored in meshes/materials.
        [[nodiscard]] static bool Load(const std::string& path, const std::string& keyPath, GltfLoadResult& result);
    };
}
