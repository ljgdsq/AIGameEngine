#pragma once

#include "Aster/Math/Math.h"

#include <array>
#include <glm/glm.hpp>
#include <functional>

namespace Aster
{
    struct Sh9
    {
        std::array<glm::vec3, 9> Coefficients{};

        // Evaluates the cosine-convolved, PI-normalized irradiance for a direction. A uniform white
        // environment evaluates to 1.0, which is the value an albedo should be multiplied by.
        [[nodiscard]] glm::vec3 Evaluate(const glm::vec3& direction) const;
        [[nodiscard]] static Sh9 Project(const std::function<glm::vec3(const glm::vec3&)>& radiance, uint32_t sampleCount);
    };

    struct PbrMaterialSample
    {
        glm::vec3 BaseColor{ 1.0f };
        float Metallic = 0.0f;
        float Roughness = 0.5f;
    };

    struct PbrLightingInput
    {
        glm::vec3 Normal{ 0.0f, 0.0f, 1.0f };
        glm::vec3 ViewDirection{ 0.0f, 0.0f, 1.0f };
        glm::vec3 LightDirection{ 0.0f, 1.0f, 0.0f };
        glm::vec3 LightColor{ 1.0f };
        PbrMaterialSample Material;
    };

    [[nodiscard]] float DistributionGGX(float nDotH, float roughness);
    [[nodiscard]] float GeometrySchlickGGX(float nDotV, float roughness);
    [[nodiscard]] float GeometrySmith(float nDotV, float nDotL, float roughness);
    [[nodiscard]] glm::vec3 FresnelSchlick(float cosTheta, const glm::vec3& f0);
    [[nodiscard]] glm::vec3 FresnelSchlickRoughness(float cosTheta, const glm::vec3& f0, float roughness);
    [[nodiscard]] glm::vec3 EvaluateDirectPbr(const PbrLightingInput& input);

    // Normalized, PI-free irradiance approximation from spherical harmonics.
    [[nodiscard]] glm::vec3 EvaluateIrradiance(const Sh9& sh, const glm::vec3& normal);

    [[nodiscard]] glm::vec3 AcesTonemap(const glm::vec3& color);
    [[nodiscard]] glm::vec3 ReinhardTonemap(const glm::vec3& color);
    [[nodiscard]] glm::vec3 LinearToSrgb(const glm::vec3& color);
    [[nodiscard]] float Luminance(const glm::vec3& color);

    [[nodiscard]] float HammersleyRadicalInverse(uint32_t bits);
    [[nodiscard]] glm::vec3 ImportanceSampleGgx(glm::vec2 xi, float roughness, const glm::vec3& normal);
}
