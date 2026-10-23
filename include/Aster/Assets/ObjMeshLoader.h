#pragma once

#include "Aster/Assets/Asset.h"

#include <memory>
#include <string>
#include <string_view>

namespace Aster
{
    class ObjMeshLoader
    {
    public:
        [[nodiscard]] static std::shared_ptr<MeshAsset> Load(const std::string& path, std::string* error = nullptr);
        [[nodiscard]] static std::shared_ptr<MeshAsset> Parse(std::string_view contents, const std::string& name,
            std::string* error = nullptr);
    };
}
