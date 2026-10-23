#pragma once

#include "Aster/Core/Base.h"

#include <filesystem>
#include <string>

namespace Aster::Primitives
{
    [[nodiscard]] std::string CubeObj();
    [[nodiscard]] std::string SphereObj(uint32 stacks = 24, uint32 slices = 32);
    [[nodiscard]] std::string PlaneObj();

    // Writes cube.obj, sphere.obj, and plane.obj into the given directory.
    [[nodiscard]] bool WriteDefaultPrimitives(const std::filesystem::path& directory,
        std::string* error = nullptr);

    // Writes a small sRGB checker texture used by the sample project.
    [[nodiscard]] bool WriteCheckerTexture(const std::filesystem::path& path, uint32 size = 128,
        std::string* error = nullptr);

    // Writes a procedural HDR equirectangular studio sky used for image-based lighting and the skybox.
    [[nodiscard]] bool WriteStudioEnvironment(const std::filesystem::path& path, uint32 width = 512,
        uint32 height = 256, std::string* error = nullptr);

    // Writes a short mono 16-bit PCM tone used by the sample project's audio source.
    [[nodiscard]] bool WriteToneWav(const std::filesystem::path& path, float frequency = 440.0f,
        float seconds = 0.5f, uint32 sampleRate = 44100, std::string* error = nullptr);
}
