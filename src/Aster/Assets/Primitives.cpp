#include "Aster/Assets/Primitives.h"

#include "Aster/Core/FileSystem.h"

#include <glm/glm.hpp>
#include <stb_image_write.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <vector>

namespace Aster::Primitives
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
    }

    std::string CubeObj()
    {
        const std::array<glm::vec3, 6> normals = {
            glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f)
        };
        const std::array<std::array<glm::vec3, 4>, 6> corners = { {
            { glm::vec3(1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 1.0f, -1.0f), glm::vec3(1.0f, 1.0f, 1.0f),
                glm::vec3(1.0f, -1.0f, 1.0f) },
            { glm::vec3(-1.0f, -1.0f, 1.0f), glm::vec3(-1.0f, 1.0f, 1.0f), glm::vec3(-1.0f, 1.0f, -1.0f),
                glm::vec3(-1.0f, -1.0f, -1.0f) },
            { glm::vec3(-1.0f, 1.0f, -1.0f), glm::vec3(-1.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f),
                glm::vec3(1.0f, 1.0f, -1.0f) },
            { glm::vec3(-1.0f, -1.0f, 1.0f), glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(1.0f, -1.0f, -1.0f),
                glm::vec3(1.0f, -1.0f, 1.0f) },
            { glm::vec3(-1.0f, -1.0f, 1.0f), glm::vec3(1.0f, -1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f),
                glm::vec3(-1.0f, 1.0f, 1.0f) },
            { glm::vec3(1.0f, -1.0f, -1.0f), glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(-1.0f, 1.0f, -1.0f),
                glm::vec3(1.0f, 1.0f, -1.0f) }
        } };
        const std::array<glm::vec2, 4> faceUVs = {
            glm::vec2(0.0f, 1.0f), glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f)
        };

        std::ostringstream stream;
        stream << "# Aster built-in cube\n";
        for (const auto& faceCorners : corners)
        {
            for (const glm::vec3& corner : faceCorners)
            {
                stream << "v " << corner.x << ' ' << corner.y << ' ' << corner.z << '\n';
            }
        }
        for (const glm::vec3& normal : normals)
        {
            stream << "vn " << normal.x << ' ' << normal.y << ' ' << normal.z << '\n';
        }
        for (size_t face = 0; face < corners.size(); ++face)
        {
            for (const glm::vec2& uv : faceUVs)
            {
                stream << "vt " << uv.x << ' ' << uv.y << '\n';
            }
        }
        for (size_t face = 0; face < corners.size(); ++face)
        {
            const size_t base = face * 4 + 1;
            const size_t uvBase = face * 4 + 1;
            stream << "f " << base << '/' << uvBase << '/' << face + 1 << ' ' << base + 1 << '/' << uvBase + 1 << '/'
                   << face + 1 << ' ' << base + 2 << '/' << uvBase + 2 << '/' << face + 1 << '\n';
            stream << "f " << base << '/' << uvBase << '/' << face + 1 << ' ' << base + 2 << '/' << uvBase + 2 << '/'
                   << face + 1 << ' ' << base + 3 << '/' << uvBase + 3 << '/' << face + 1 << '\n';
        }
        return stream.str();
    }

    std::string SphereObj(uint32 stacks, uint32 slices)
    {
        stacks = std::max(stacks, 3U);
        slices = std::max(slices, 3U);

        std::ostringstream stream;
        stream << "# Aster built-in sphere\n";
        for (uint32 stack = 0; stack <= stacks; ++stack)
        {
            const float v = static_cast<float>(stack) / static_cast<float>(stacks);
            const float phi = v * Pi;
            for (uint32 slice = 0; slice <= slices; ++slice)
            {
                const float u = static_cast<float>(slice) / static_cast<float>(slices);
                const float theta = u * 2.0f * Pi;
                const glm::vec3 position(std::sin(phi) * std::cos(theta), std::cos(phi),
                    std::sin(phi) * std::sin(theta));
                stream << "v " << position.x << ' ' << position.y << ' ' << position.z << '\n';
                stream << "vn " << position.x << ' ' << position.y << ' ' << position.z << '\n';
                stream << "vt " << u << ' ' << 1.0f - v << '\n';
            }
        }

        for (uint32 stack = 0; stack < stacks; ++stack)
        {
            for (uint32 slice = 0; slice < slices; ++slice)
            {
                const uint32 first = stack * (slices + 1) + slice + 1;
                const uint32 second = first + slices + 1;
                stream << "f " << first << '/' << first << '/' << first << ' ' << second << '/' << second << '/'
                       << second << ' ' << first + 1 << '/' << first + 1 << '/' << first + 1 << '\n';
                stream << "f " << first + 1 << '/' << first + 1 << '/' << first + 1 << ' ' << second << '/'
                       << second << '/' << second << ' ' << second + 1 << '/' << second + 1 << '/' << second + 1
                       << '\n';
            }
        }
        return stream.str();
    }

    std::string PlaneObj()
    {
        std::ostringstream stream;
        stream << "# Aster built-in plane\n";
        stream << "v -0.5 0 -0.5\nv -0.5 0 0.5\nv 0.5 0 0.5\nv 0.5 0 -0.5\n";
        stream << "vn 0 1 0\n";
        stream << "vt 0 1\nvt 0 0\nvt 1 0\nvt 1 1\n";
        stream << "f 1/1/1 2/2/1 3/3/1\n";
        stream << "f 1/1/1 3/3/1 4/4/1\n";
        return stream.str();
    }

    bool WriteDefaultPrimitives(const std::filesystem::path& directory, std::string* error)
    {
        if (!FileSystem::CreateDirectories(directory))
        {
            if (error != nullptr)
            {
                *error = "Could not create the model directory: " + FileSystem::Normalize(directory);
            }
            return false;
        }

        const std::pair<const char*, std::string> files[] = {
            { "cube.obj", CubeObj() },
            { "sphere.obj", SphereObj() },
            { "plane.obj", PlaneObj() }
        };
        for (const auto& [name, contents] : files)
        {
            if (!FileSystem::WriteTextFile(directory / name, contents))
            {
                if (error != nullptr)
                {
                    *error = std::string("Could not write primitive asset: ") + name;
                }
                return false;
            }
        }
        return true;
    }

    bool WriteCheckerTexture(const std::filesystem::path& path, uint32 size, std::string* error)
    {
        if (size == 0)
        {
            if (error != nullptr)
            {
                *error = "Checker texture size must be positive";
            }
            return false;
        }
        if (!FileSystem::CreateDirectories(path.parent_path()))
        {
            if (error != nullptr)
            {
                *error = "Could not create the texture directory: " + FileSystem::Normalize(path.parent_path());
            }
            return false;
        }

        std::vector<uint8> pixels(static_cast<size_t>(size) * size * 4U);
        for (uint32 y = 0; y < size; ++y)
        {
            for (uint32 x = 0; x < size; ++x)
            {
                const bool light = ((x / 8U) + (y / 8U)) % 2U == 0U;
                uint8 color[3] = { 48, 56, 70 };
                if (light)
                {
                    color[0] = 166;
                    color[1] = 176;
                    color[2] = 190;
                }
                if (x % 32U == 0U || y % 32U == 0U)
                {
                    color[0] = static_cast<uint8>(color[0] * 3U / 4U);
                    color[1] = static_cast<uint8>(color[1] * 3U / 4U);
                    color[2] = static_cast<uint8>(color[2] * 3U / 4U);
                }
                const size_t offset = (static_cast<size_t>(y) * size + x) * 4U;
                pixels[offset + 0] = color[0];
                pixels[offset + 1] = color[1];
                pixels[offset + 2] = color[2];
                pixels[offset + 3] = 255;
            }
        }

        if (stbi_write_png(path.string().c_str(), static_cast<int>(size), static_cast<int>(size), 4,
                pixels.data(), static_cast<int>(size) * 4) == 0)
        {
            if (error != nullptr)
            {
                *error = "Could not write checker texture: " + FileSystem::Normalize(path);
            }
            return false;
        }
        return true;
    }
}
