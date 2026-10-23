#include "Aster/Rendering/Ibl.h"

#include <algorithm>
#include <cmath>

namespace Aster
{
    namespace
    {
        glm::vec3 DecodePixel(const Image& image, uint32_t x, uint32_t y)
        {
            if (x >= image.Width || y >= image.Height)
            {
                return glm::vec3(0.0f);
            }
            if (image.Format == ImageFormat::RGBA8)
            {
                const uint8_t* pixel = image.Data.data() + (static_cast<size_t>(y) * image.Width + x) * 4;
                glm::vec3 color(pixel[0], pixel[1], pixel[2]);
                color /= 255.0f;
                return glm::pow(color, glm::vec3(2.2f));
            }
            const float* pixel = reinterpret_cast<const float*>(image.Data.data()) +
                (static_cast<size_t>(y) * image.Width + x) * 4;
            return glm::max(glm::vec3(pixel[0], pixel[1], pixel[2]), glm::vec3(0.0f));
        }

        glm::vec3 SampleBilinear(const Image& image, float u, float v)
        {
            if (!image.IsValid())
            {
                return glm::vec3(0.0f);
            }
            u = std::clamp(u, 0.0f, 1.0f);
            v = std::clamp(v, 0.0f, 1.0f);
            const float x = u * static_cast<float>(image.Width - 1);
            const float y = v * static_cast<float>(image.Height - 1);
            const uint32_t x0 = static_cast<uint32_t>(std::floor(x));
            const uint32_t y0 = static_cast<uint32_t>(std::floor(y));
            const uint32_t x1 = std::min(x0 + 1, image.Width - 1);
            const uint32_t y1 = std::min(y0 + 1, image.Height - 1);
            const float tx = x - static_cast<float>(x0);
            const float ty = y - static_cast<float>(y0);

            const glm::vec3 top = glm::mix(DecodePixel(image, x0, y0), DecodePixel(image, x1, y0), tx);
            const glm::vec3 bottom = glm::mix(DecodePixel(image, x0, y1), DecodePixel(image, x1, y1), tx);
            return glm::mix(top, bottom, ty);
        }

        glm::vec2 Hammersley(uint32_t index, uint32_t count)
        {
            return glm::vec2(static_cast<float>(index) / static_cast<float>(count),
                HammersleyRadicalInverse(index));
        }
    }

    glm::vec3 IblBaker::CubemapDirection(uint32_t face, float u, float v)
    {
        // u and v are in [-1, 1] and follow the Direct3D cube map face order:
        // +X, -X, +Y, -Y, +Z, -Z.
        switch (face)
        {
            case 0: return SafeNormalize(glm::vec3(1.0f, -v, -u));
            case 1: return SafeNormalize(glm::vec3(-1.0f, -v, u));
            case 2: return SafeNormalize(glm::vec3(u, 1.0f, v));
            case 3: return SafeNormalize(glm::vec3(u, -1.0f, -v));
            case 4: return SafeNormalize(glm::vec3(u, -v, 1.0f));
            default: return SafeNormalize(glm::vec3(-u, -v, -1.0f));
        }
    }

    glm::vec3 IblBaker::SampleEquirectangular(const Image& image, const glm::vec3& direction)
    {
        const glm::vec3 normalized = SafeNormalize(direction, glm::vec3(0.0f, 0.0f, 1.0f));
        const float u = std::atan2(normalized.z, normalized.x) / TwoPi + 0.5f;
        const float v = std::acos(std::clamp(normalized.y, -1.0f, 1.0f)) / Pi;
        return SampleBilinear(image, u, v);
    }

    Sh9 IblBaker::ProjectIrradiance(const Image& image, uint32_t sampleCount)
    {
        return Sh9::Project(
            [&image](const glm::vec3& direction) { return SampleEquirectangular(image, direction); },
            sampleCount);
    }

    void IblBaker::BuildBrdfLut(uint32_t size, std::vector<glm::vec2>& output)
    {
        size = std::max(2U, size);
        output.assign(static_cast<size_t>(size) * size, glm::vec2(0.0f));
        const uint32_t sampleCount = 256;

        for (uint32_t y = 0; y < size; ++y)
        {
            const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(size);
            for (uint32_t x = 0; x < size; ++x)
            {
                const float nDotV = (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
                const glm::vec3 view(std::sqrt(std::max(0.0f, 1.0f - nDotV * nDotV)), 0.0f, nDotV);
                float scale = 0.0f;
                float bias = 0.0f;

                for (uint32_t sample = 0; sample < sampleCount; ++sample)
                {
                    const glm::vec2 xi = Hammersley(sample, sampleCount);
                    const glm::vec3 halfVector = ImportanceSampleGgx(xi, roughness, glm::vec3(0.0f, 0.0f, 1.0f));
                    const glm::vec3 light = SafeNormalize(2.0f * glm::dot(view, halfVector) * halfVector - view);
                    const float nDotL = std::max(light.z, 0.0f);
                    const float nDotH = std::max(halfVector.z, 0.0f);
                    const float vDotH = std::max(glm::dot(view, halfVector), 0.0f);
                    if (nDotL <= 0.0f)
                    {
                        continue;
                    }

                    const float geometry = GeometrySmith(nDotV, nDotL, roughness);
                    const float visibility = geometry * vDotH / std::max(nDotH * nDotV, Epsilon);
                    const float fresnel = std::pow(1.0f - vDotH, 5.0f);
                    scale += (1.0f - fresnel) * visibility;
                    bias += fresnel * visibility;
                }

                output[static_cast<size_t>(y) * size + x] =
                    glm::vec2(scale / static_cast<float>(sampleCount), bias / static_cast<float>(sampleCount));
            }
        }
    }

    bool IblBaker::Bake(const Image& equirectangular, Environment& environment, std::string* error)
    {
        if (!equirectangular.IsValid())
        {
            if (error != nullptr)
            {
                *error = "Environment image is empty";
            }
            return false;
        }

        constexpr uint32_t prefilterSize = 64;
        constexpr uint32_t mipCount = 6;

        CubemapImage prefiltered;
        prefiltered.FaceSize = prefilterSize;
        prefiltered.MipCount = mipCount;

        size_t totalPixels = 0;
        for (uint32_t mip = 0; mip < mipCount; ++mip)
        {
            const uint32_t mipSize = std::max(1U, prefilterSize >> mip);
            totalPixels += static_cast<size_t>(6) * mipSize * mipSize;
        }
        prefiltered.Pixels.resize(totalPixels);

        constexpr uint32_t sampleCount = 64;
        size_t writeOffset = 0;
        for (uint32_t face = 0; face < 6; ++face)
        {
            for (uint32_t mip = 0; mip < mipCount; ++mip)
            {
                const uint32_t mipSize = std::max(1U, prefilterSize >> mip);
                const float roughness = mipCount > 1
                    ? static_cast<float>(mip) / static_cast<float>(mipCount - 1)
                    : 0.0f;
                for (uint32_t y = 0; y < mipSize; ++y)
                {
                    for (uint32_t x = 0; x < mipSize; ++x)
                    {
                        const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(mipSize) * 2.0f - 1.0f;
                        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(mipSize) * 2.0f - 1.0f;
                        const glm::vec3 normal = CubemapDirection(face, u, v);

                        glm::vec3 color;
                        if (roughness < 0.001f)
                        {
                            color = SampleEquirectangular(equirectangular, normal);
                        }
                        else
                        {
                            const glm::vec3 view = normal;
                            glm::vec3 accumulated(0.0f);
                            float weight = 0.0f;
                            for (uint32_t sample = 0; sample < sampleCount; ++sample)
                            {
                                const glm::vec2 xi = Hammersley(sample, sampleCount);
                                const glm::vec3 halfVector = ImportanceSampleGgx(xi, roughness, normal);
                                const glm::vec3 light = SafeNormalize(2.0f * glm::dot(view, halfVector) * halfVector - view);
                                const float nDotL = glm::dot(normal, light);
                                if (nDotL <= 0.0f)
                                {
                                    continue;
                                }
                                accumulated += SampleEquirectangular(equirectangular, light) * nDotL;
                                weight += nDotL;
                            }
                            color = weight > 0.0f ? accumulated / weight : SampleEquirectangular(equirectangular, normal);
                        }

                        prefiltered.Pixels[writeOffset++] = glm::vec4(color, 1.0f);
                    }
                }
            }
        }

        environment.PrefilteredSpecular = std::move(prefiltered);
        environment.Equirectangular = equirectangular;
        environment.IrradianceSh = ProjectIrradiance(equirectangular).Coefficients;
        BuildBrdfLut(128, environment.BrdfLut);
        environment.BrdfLutSize = 128;
        return true;
    }
}
