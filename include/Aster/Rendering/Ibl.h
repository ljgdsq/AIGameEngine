#pragma once

#include "Aster/Assets/Asset.h"
#include "Aster/Rendering/Pbr.h"

#include <string>
#include <vector>

namespace Aster
{
    class IblBaker
    {
    public:
        [[nodiscard]] static bool Bake(const Image& equirectangular, Environment& environment,
            std::string* error = nullptr);

        [[nodiscard]] static glm::vec3 SampleEquirectangular(const Image& image, const glm::vec3& direction);
        [[nodiscard]] static glm::vec3 CubemapDirection(uint32_t face, float u, float v);
        [[nodiscard]] static Sh9 ProjectIrradiance(const Image& image, uint32_t sampleCount = 4096);
        static void BuildBrdfLut(uint32_t size, std::vector<glm::vec2>& output);
    };
}
