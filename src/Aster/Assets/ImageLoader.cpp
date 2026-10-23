#include "Aster/Assets/ImageLoader.h"

#include "Aster/Core/FileSystem.h"

#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Aster
{
    namespace
    {
        void SetError(std::string* error, const char* text)
        {
            if (error != nullptr)
            {
                *error = text != nullptr ? text : "unknown image error";
            }
        }
    }

    glm::vec4 Image::Sample(float u, float v) const
    {
        if (!IsValid())
        {
            return glm::vec4(0.0f);
        }

        u = u - std::floor(u);
        v = v - std::floor(v);
        const uint32 x = std::min(static_cast<uint32>(u * static_cast<float>(Width)), Width - 1);
        const uint32 y = std::min(static_cast<uint32>(v * static_cast<float>(Height)), Height - 1);
        const size_t index = (static_cast<size_t>(y) * Width + x);
        if (Format == ImageFormat::RGBA8)
        {
            const uint8* pixel = Data.data() + index * 4;
            return glm::vec4(pixel[0], pixel[1], pixel[2], pixel[3]) / 255.0f;
        }
        const float* pixel = reinterpret_cast<const float*>(Data.data()) + index * 4;
        return glm::vec4(pixel[0], pixel[1], pixel[2], pixel[3]);
    }

    std::optional<Image> ImageLoader::Load(const std::string& path, std::string* error)
    {
        if (!FileSystem::Exists(path))
        {
            SetError(error, "image file does not exist");
            return std::nullopt;
        }

        const std::string extension = FileSystem::GetExtension(path);
        const bool hdr = extension == "hdr";
        if (hdr)
        {
            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_set_flip_vertically_on_load(0);
            float* pixels = stbi_loadf(path.c_str(), &width, &height, &channels, 4);
            if (pixels == nullptr)
            {
                SetError(error, stbi_failure_reason());
                return std::nullopt;
            }

            Image image;
            image.Width = static_cast<uint32>(width);
            image.Height = static_cast<uint32>(height);
            image.Format = ImageFormat::RGBA32F;
            image.Data.resize(static_cast<size_t>(width) * height * 4U * sizeof(float));
            std::memcpy(image.Data.data(), pixels, image.Data.size());
            stbi_image_free(pixels);
            return image;
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_set_flip_vertically_on_load(0);
        stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
        if (pixels == nullptr)
        {
            SetError(error, stbi_failure_reason());
            return std::nullopt;
        }

        Image image;
        image.Width = static_cast<uint32>(width);
        image.Height = static_cast<uint32>(height);
        image.Format = ImageFormat::RGBA8;
        image.Data.assign(pixels, pixels + static_cast<size_t>(width) * height * 4U);
        stbi_image_free(pixels);
        return image;
    }

    std::optional<Image> ImageLoader::LoadFromMemory(const uint8* data, size_t size, bool hdr, bool flipVertically,
        std::string* error)
    {
        if (data == nullptr || size == 0)
        {
            SetError(error, "empty image data");
            return std::nullopt;
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);

        Image image;
        if (hdr)
        {
            float* pixels = stbi_loadf_from_memory(data, static_cast<int>(size), &width, &height, &channels, 4);
            if (pixels == nullptr)
            {
                SetError(error, stbi_failure_reason());
                return std::nullopt;
            }
            image.Width = static_cast<uint32>(width);
            image.Height = static_cast<uint32>(height);
            image.Format = ImageFormat::RGBA32F;
            image.Data.resize(static_cast<size_t>(width) * height * 4U * sizeof(float));
            std::memcpy(image.Data.data(), pixels, image.Data.size());
            stbi_image_free(pixels);
        }
        else
        {
            stbi_uc* pixels = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &channels, 4);
            if (pixels == nullptr)
            {
                SetError(error, stbi_failure_reason());
                return std::nullopt;
            }
            image.Width = static_cast<uint32>(width);
            image.Height = static_cast<uint32>(height);
            image.Format = ImageFormat::RGBA8;
            image.Data.assign(pixels, pixels + static_cast<size_t>(width) * height * 4U);
            stbi_image_free(pixels);
        }

        stbi_set_flip_vertically_on_load(0);
        return image;
    }

    bool ImageLoader::WritePng(const std::string& path, const Image& image, std::string* error)
    {
        if (!image.IsValid() || image.Format != ImageFormat::RGBA8)
        {
            SetError(error, "only valid RGBA8 images can be written as PNG");
            return false;
        }

        const std::string directory = FileSystem::Normalize(FileSystem::GetDirectory(path));
        if (!directory.empty() && !FileSystem::CreateDirectories(directory))
        {
            SetError(error, "could not create output directory");
            return false;
        }

        if (stbi_write_png(path.c_str(), static_cast<int>(image.Width), static_cast<int>(image.Height), 4,
                image.Data.data(), static_cast<int>(image.GetRowPitch())) == 0)
        {
            SetError(error, "stb_image_write failed");
            return false;
        }
        return true;
    }
}
