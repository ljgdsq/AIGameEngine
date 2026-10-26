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
        stream << "# Aster built-in cube (unit size, half extent 0.5)\n";
        for (const auto& faceCorners : corners)
        {
            for (const glm::vec3& corner : faceCorners)
            {
                // Authored primitives are one unit across so Transform scale maps directly to
                // meters. Collider half extents of 0.5 therefore match a scale-1 cube.
                stream << "v " << corner.x * 0.5f << ' ' << corner.y * 0.5f << ' ' << corner.z * 0.5f << '\n';
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
        stream << "# Aster built-in sphere (unit diameter, radius 0.5)\n";
        for (uint32 stack = 0; stack <= stacks; ++stack)
        {
            const float v = static_cast<float>(stack) / static_cast<float>(stacks);
            const float phi = v * Pi;
            for (uint32 slice = 0; slice <= slices; ++slice)
            {
                const float u = static_cast<float>(slice) / static_cast<float>(slices);
                const float theta = u * 2.0f * Pi;
                const glm::vec3 direction(std::sin(phi) * std::cos(theta), std::cos(phi),
                    std::sin(phi) * std::sin(theta));
                const glm::vec3 position = direction * 0.5f;
                stream << "v " << position.x << ' ' << position.y << ' ' << position.z << '\n';
                // Normals stay unit length; only the sphere surface is scaled.
                stream << "vn " << direction.x << ' ' << direction.y << ' ' << direction.z << '\n';
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

    bool WriteStudioEnvironment(const std::filesystem::path& path, uint32 width, uint32 height, std::string* error)
    {
        if (width == 0 || height == 0)
        {
            if (error != nullptr)
            {
                *error = "Environment dimensions must be positive";
            }
            return false;
        }

        const glm::vec3 sunDirection = glm::normalize(glm::vec3(0.35f, 0.55f, -0.5f));
        std::vector<float> pixels(static_cast<size_t>(width) * height * 3);
        for (uint32 y = 0; y < height; ++y)
        {
            const float latitude = (0.5f - (static_cast<float>(y) + 0.5f) / static_cast<float>(height)) * Pi;
            for (uint32 x = 0; x < width; ++x)
            {
                const float longitude = ((static_cast<float>(x) + 0.5f) / static_cast<float>(width) * 2.0f - 1.0f) *
                    Pi;
                const glm::vec3 direction(std::cos(latitude) * std::sin(longitude), std::sin(latitude),
                    std::cos(latitude) * std::cos(longitude));

                const glm::vec3 zenith(0.16f, 0.27f, 0.46f);
                const glm::vec3 horizon(1.05f, 0.72f, 0.42f);
                const glm::vec3 ground(0.045f, 0.04f, 0.035f);
                glm::vec3 color;
                if (direction.y >= 0.0f)
                {
                    color = glm::mix(horizon, zenith, std::pow(direction.y, 0.45f));
                }
                else
                {
                    color = glm::mix(horizon, ground, std::pow(-direction.y, 0.55f));
                }
                const float horizonGlow = std::exp(-std::abs(direction.y) * 6.0f);
                color += horizon * horizonGlow * 0.22f;
                const float sun = std::pow(std::max(glm::dot(direction, sunDirection), 0.0f), 900.0f) * 70.0f;
                color += glm::vec3(1.0f, 0.78f, 0.48f) * sun;

                const size_t offset = (static_cast<size_t>(y) * width + x) * 3;
                pixels[offset + 0] = color.r;
                pixels[offset + 1] = color.g;
                pixels[offset + 2] = color.b;
            }
        }

        if (stbi_write_hdr(path.string().c_str(), static_cast<int>(width), static_cast<int>(height), 3,
                pixels.data()) == 0)
        {
            if (error != nullptr)
            {
                *error = "Could not write environment image: " + FileSystem::Normalize(path);
            }
            return false;
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

    bool WriteToneWav(const std::filesystem::path& path, float frequency, float seconds, uint32 sampleRate,
        std::string* error)
    {
        if (sampleRate < 8000 || sampleRate > 192000 || seconds <= 0.0f || seconds > 30.0f)
        {
            if (error != nullptr)
            {
                *error = "Invalid tone parameters";
            }
            return false;
        }

        const uint32 frameCount = static_cast<uint32>(static_cast<float>(sampleRate) * seconds);
        const uint32 dataSize = frameCount * 2U;
        std::vector<uint8> wav(44U + dataSize, 0);
        const auto writeU16 = [&wav](size_t offset, uint16 value)
        {
            wav[offset] = static_cast<uint8>(value & 0xFF);
            wav[offset + 1] = static_cast<uint8>((value >> 8) & 0xFF);
        };
        const auto writeU32 = [&wav](size_t offset, uint32 value)
        {
            wav[offset] = static_cast<uint8>(value & 0xFF);
            wav[offset + 1] = static_cast<uint8>((value >> 8) & 0xFF);
            wav[offset + 2] = static_cast<uint8>((value >> 16) & 0xFF);
            wav[offset + 3] = static_cast<uint8>((value >> 24) & 0xFF);
        };

        wav[0] = 'R';
        wav[1] = 'I';
        wav[2] = 'F';
        wav[3] = 'F';
        writeU32(4, 36U + dataSize);
        wav[8] = 'W';
        wav[9] = 'A';
        wav[10] = 'V';
        wav[11] = 'E';
        wav[12] = 'f';
        wav[13] = 'm';
        wav[14] = 't';
        wav[15] = ' ';
        writeU32(16, 16);
        writeU16(20, 1);
        writeU16(22, 1);
        writeU32(24, sampleRate);
        writeU32(28, sampleRate * 2U);
        writeU16(32, 2);
        writeU16(34, 16);
        wav[36] = 'd';
        wav[37] = 'a';
        wav[38] = 't';
        wav[39] = 'a';
        writeU32(40, dataSize);

        const float fade = static_cast<float>(sampleRate) * 0.01f;
        for (uint32 frame = 0; frame < frameCount; ++frame)
        {
            const float t = static_cast<float>(frame) / static_cast<float>(sampleRate);
            float envelope = 1.0f;
            if (static_cast<float>(frame) < fade)
            {
                envelope = static_cast<float>(frame) / fade;
            }
            else if (static_cast<float>(frameCount - frame) < fade)
            {
                envelope = static_cast<float>(frameCount - frame) / fade;
            }
            const float sample = std::sin(6.2831853f * frequency * t) * 0.25f * envelope;
            const int16 value = static_cast<int16>(std::clamp(sample, -1.0f, 1.0f) * 32767.0f);
            writeU16(44U + static_cast<size_t>(frame) * 2U, static_cast<uint16>(value));
        }

        if (!FileSystem::CreateDirectories(path.parent_path()) ||
            !FileSystem::WriteBinaryFile(path, wav.data(), wav.size()))
        {
            if (error != nullptr)
            {
                *error = "Could not write tone WAV: " + FileSystem::Normalize(path);
            }
            return false;
        }
        return true;
    }
}
