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
}
