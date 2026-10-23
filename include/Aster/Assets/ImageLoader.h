#pragma once

#include "Aster/Assets/Asset.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Aster
{
    class ImageLoader
    {
    public:
        [[nodiscard]] static std::optional<Image> Load(const std::string& path, std::string* error = nullptr);
        [[nodiscard]] static std::optional<Image> LoadFromMemory(const uint8* data, size_t size, bool hdr,
            bool flipVertically, std::string* error = nullptr);
        [[nodiscard]] static bool WritePng(const std::string& path, const Image& image, std::string* error = nullptr);
    };
}
