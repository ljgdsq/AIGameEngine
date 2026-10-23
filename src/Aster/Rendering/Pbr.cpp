#include "Aster/Rendering/Pbr.h"

#include <algorithm>
#include <cmath>

namespace Aster
{
    namespace
    {
        constexpr float ShBasis0 = 0.28209479177387814f;
        constexpr float ShBasis1 = 0.4886025119029199f;
        constexpr float ShBasis2 = 1.0925484305920792f;
        constexpr float ShBasis3 = 0.31539156525252005f;
        constexpr float ShBasis4 = 0.5462742152960396f;

        void EvaluateShBasis(const glm::vec3& direction, std::array<float, 9>& basis)
        {
            const float x = direction.x;
            const float y = direction.y;
            const float z = direction.z;
            basis[0] = ShBasis0;
            basis[1] = ShBasis1 * y;
            basis[2] = ShBasis1 * z;
            basis[3] = ShBasis1 * x;
            basis[4] = ShBasis2 * x * y;
            basis[5] = ShBasis2 * y * z;
            basis[6] = ShBasis3 * (3.0f * z * z - 1.0f);
            basis[7] = ShBasis2 * x * z;
            basis[8] = ShBasis4 * (x * x - y * y);
        }
    }

    glm::vec3 Sh9::Evaluate(const glm::vec3& direction) const
    {
        std::array<float, 9> basis{};
        EvaluateShBasis(SafeNormalize(direction, glm::vec3(0.0f, 0.0f, 1.0f)), basis);
        glm::vec3 result(0.0f);
        for (size_t index = 0; index < basis.size(); ++index)
        {
            result += Coefficients[index] * basis[index];
        }
        return glm::max(result, glm::vec3(0.0f));
    }

    Sh9 Sh9::Project(const std::function<glm::vec3(const glm::vec3&)>& radiance, uint32_t sampleCount)
    {
        Sh9 result;
        if (!radiance || sampleCount == 0)
        {
            return result;
        }

        const uint32_t sqrtCount = std::max(2U, static_cast<uint32_t>(std::sqrt(static_cast<float>(sampleCount))));
        const float goldenAngle = Pi * (3.0f - std::sqrt(5.0f));
        for (uint32_t index = 0; index < sqrtCount * sqrtCount; ++index)
        {
            const float y = 1.0f - 2.0f * (static_cast<float>(index) + 0.5f) / static_cast<float>(sqrtCount * sqrtCount);
            const float radius = std::sqrt(std::max(0.0f, 1.0f - y * y));
            const float theta = goldenAngle * static_cast<float>(index);
            const glm::vec3 direction(std::cos(theta) * radius, y, std::sin(theta) * radius);

            const glm::vec3 color = radiance(direction);
            const float weight = 4.0f * Pi / static_cast<float>(sqrtCount * sqrtCount);
            std::array<float, 9> basis{};
            EvaluateShBasis(direction, basis);
            for (size_t coefficient = 0; coefficient < basis.size(); ++coefficient)
            {
                result.Coefficients[coefficient] += color * (basis[coefficient] * weight);
            }
        }

        // Convolve with the clamped cosine kernel and divide by PI so a constant white
        // environment evaluates to 1.0.
        const float factors[9] = { 1.0f, 2.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f };
        for (size_t index = 0; index < result.Coefficients.size(); ++index)
        {
            result.Coefficients[index] *= factors[index];
        }
        return result;
    }

    float DistributionGGX(float nDotH, float roughness)
    {
        const float alpha = roughness * roughness;
        const float alphaSquared = alpha * alpha;
        const float denominator = nDotH * nDotH * (alphaSquared - 1.0f) + 1.0f;
        return alphaSquared / std::max(Pi * denominator * denominator, Epsilon);
    }

    float GeometrySchlickGGX(float nDotV, float roughness)
    {
        const float r = roughness + 1.0f;
        const float k = (r * r) / 8.0f;
        return nDotV / std::max(nDotV * (1.0f - k) + k, Epsilon);
    }

    float GeometrySmith(float nDotV, float nDotL, float roughness)
    {
        return GeometrySchlickGGX(nDotV, roughness) * GeometrySchlickGGX(nDotL, roughness);
    }

    glm::vec3 FresnelSchlick(float cosTheta, const glm::vec3& f0)
    {
        const float factor = std::pow(std::clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
        return f0 + (glm::vec3(1.0f) - f0) * factor;
    }

    glm::vec3 FresnelSchlickRoughness(float cosTheta, const glm::vec3& f0, float roughness)
    {
        const glm::vec3 maximum = glm::max(glm::vec3(1.0f - roughness), f0);
        const float factor = std::pow(std::clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
        return f0 + (maximum - f0) * factor;
    }

    DirectionalShadowMatrix BuildDirectionalShadowMatrix(const glm::vec3& center, const glm::vec3& extents,
        const glm::vec3& lightDirection)
    {
        DirectionalShadowMatrix result;
        result.Radius = (std::max)(glm::length(extents), 4.0f);
        const glm::vec3 direction = SafeNormalize(lightDirection, glm::vec3(0.0f, -1.0f, 0.0f));
        const glm::vec3 up = glm::abs(direction.y) > 0.95f ? glm::vec3(0.0f, 0.0f, 1.0f) :
            glm::vec3(0.0f, 1.0f, 0.0f);
        const float distance = result.Radius * 2.0f + 10.0f;
        const glm::mat4 view = glm::lookAt(center - direction * distance, center, up);
        const glm::mat4 projection = glm::orthoRH_ZO(-result.Radius, result.Radius, -result.Radius,
            result.Radius, 0.1f, distance + result.Radius * 2.0f);
        result.ViewProjection = projection * view;
        return result;
    }

    glm::vec3 EvaluateDirectPbr(const PbrLightingInput& input)
    {
        const glm::vec3 normal = SafeNormalize(input.Normal, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::vec3 view = SafeNormalize(input.ViewDirection, glm::vec3(0.0f, 0.0f, 1.0f));
        const glm::vec3 light = SafeNormalize(input.LightDirection, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::vec3 halfVector = SafeNormalize(view + light, normal);

        const float nDotV = std::max(glm::dot(normal, view), 0.0f);
        const float nDotL = std::max(glm::dot(normal, light), 0.0f);
        const float nDotH = std::max(glm::dot(normal, halfVector), 0.0f);
        const float vDotH = std::max(glm::dot(view, halfVector), 0.0f);
        if (nDotL <= 0.0f || nDotV <= 0.0f)
        {
            return glm::vec3(0.0f);
        }

        const float roughness = std::clamp(input.Material.Roughness, 0.04f, 1.0f);
        const float metallic = std::clamp(input.Material.Metallic, 0.0f, 1.0f);
        const glm::vec3 baseColor = glm::max(input.Material.BaseColor, glm::vec3(0.0f));
        const glm::vec3 f0 = glm::mix(glm::vec3(0.04f), baseColor, metallic);
        const glm::vec3 fresnel = FresnelSchlick(vDotH, f0);

        const float distribution = DistributionGGX(nDotH, roughness);
        const float geometry = GeometrySmith(nDotV, nDotL, roughness);
        const glm::vec3 specular = (distribution * geometry * fresnel) / std::max(4.0f * nDotV * nDotL, Epsilon);
        const glm::vec3 diffuse = (glm::vec3(1.0f) - fresnel) * (1.0f - metallic) * baseColor / Pi;
        return (diffuse + specular) * input.LightColor * nDotL;
    }

    glm::vec3 EvaluateIrradiance(const Sh9& sh, const glm::vec3& normal)
    {
        return sh.Evaluate(normal);
    }

    glm::vec3 AcesTonemap(const glm::vec3& color)
    {
        constexpr float a = 2.51f;
        constexpr float b = 0.03f;
        constexpr float c = 2.43f;
        constexpr float d = 0.59f;
        constexpr float e = 0.14f;
        const glm::vec3 numerator = color * (glm::vec3(a) * color + glm::vec3(b));
        const glm::vec3 denominator = color * (glm::vec3(c) * color + glm::vec3(d)) + glm::vec3(e);
        return glm::clamp(numerator / denominator, glm::vec3(0.0f), glm::vec3(1.0f));
    }

    glm::vec3 ReinhardTonemap(const glm::vec3& color)
    {
        return color / (glm::vec3(1.0f) + color);
    }

    glm::vec3 LinearToSrgb(const glm::vec3& color)
    {
        const glm::vec3 low = color * 12.92f;
        const glm::vec3 high = glm::vec3(1.055f) * glm::pow(glm::max(color, glm::vec3(0.0f)), glm::vec3(1.0f / 2.4f)) - glm::vec3(0.055f);
        return glm::mix(low, high, glm::greaterThan(color, glm::vec3(0.0031308f)));
    }

    float Luminance(const glm::vec3& color)
    {
        return 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b;
    }

    float HammersleyRadicalInverse(uint32_t bits)
    {
        bits = (bits << 16U) | (bits >> 16U);
        bits = ((bits & 0x55555555U) << 1U) | ((bits & 0xAAAAAAAAU) >> 1U);
        bits = ((bits & 0x33333333U) << 2U) | ((bits & 0xCCCCCCCCU) >> 2U);
        bits = ((bits & 0x0F0F0F0FU) << 4U) | ((bits & 0xF0F0F0F0U) >> 4U);
        bits = ((bits & 0x00FF00FFU) << 8U) | ((bits & 0xFF00FF00U) >> 8U);
        return static_cast<float>(bits) * 2.3283064365386963e-10f;
    }

    glm::vec3 ImportanceSampleGgx(glm::vec2 xi, float roughness, const glm::vec3& normal)
    {
        const float a = roughness * roughness;
        const float phi = TwoPi * xi.x;
        const float cosTheta = std::sqrt((1.0f - xi.y) / (1.0f + (a * a - 1.0f) * xi.y));
        const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

        const glm::vec3 halfVector(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta);
        const glm::vec3 up = std::abs(normal.z) < 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 tangent = SafeNormalize(glm::cross(up, normal), glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::vec3 bitangent = glm::cross(normal, tangent);
        return SafeNormalize(tangent * halfVector.x + bitangent * halfVector.y + normal * halfVector.z);
    }
}
