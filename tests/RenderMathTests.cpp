#include "TestFramework.h"

#include "Aster/Rendering/Camera.h"
#include "Aster/Rendering/Ibl.h"
#include "Aster/Rendering/Pbr.h"

#include <cmath>

using namespace Aster;

ASTER_TEST(RenderMath, PbrBrdfIsFiniteAndPositive)
{
    PbrLightingInput input;
    input.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
    input.ViewDirection = glm::vec3(0.0f, 1.0f, 0.0f);
    input.LightDirection = glm::vec3(0.0f, 1.0f, 0.0f);
    input.LightColor = glm::vec3(3.0f);
    input.Material.BaseColor = glm::vec3(0.8f, 0.2f, 0.1f);
    input.Material.Metallic = 0.2f;
    input.Material.Roughness = 0.4f;

    const glm::vec3 color = EvaluateDirectPbr(input);
    ASTER_CHECK(std::isfinite(color.r));
    ASTER_CHECK(color.r > 0.0f);
    ASTER_CHECK(color.g >= 0.0f);
    ASTER_CHECK(color.b >= 0.0f);
}

ASTER_TEST(RenderMath, PbrIsBlackWhenLightIsBehind)
{
    PbrLightingInput input;
    input.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
    input.ViewDirection = glm::vec3(0.0f, 1.0f, 0.0f);
    input.LightDirection = glm::vec3(0.0f, -1.0f, 0.0f);
    const glm::vec3 color = EvaluateDirectPbr(input);
    ASTER_CHECK_NEAR(glm::length(color), 0.0, 1e-6);
}

ASTER_TEST(RenderMath, GGXAndSmithAreFinite)
{
    for (float roughness : { 0.05f, 0.5f, 1.0f })
    {
        const float distribution = DistributionGGX(0.9f, roughness);
        const float geometry = GeometrySmith(0.8f, 0.7f, roughness);
        ASTER_CHECK(std::isfinite(distribution));
        ASTER_CHECK(distribution >= 0.0f);
        ASTER_CHECK(std::isfinite(geometry));
        ASTER_CHECK(geometry >= 0.0f && geometry <= 1.0f);
    }
}

ASTER_TEST(RenderMath, Tonemapping)
{
    ASTER_CHECK_NEAR(glm::length(AcesTonemap(glm::vec3(0.0f))), 0.0, 1e-6);
    const glm::vec3 mapped = AcesTonemap(glm::vec3(10.0f));
    ASTER_CHECK(mapped.r <= 1.0f && mapped.g <= 1.0f && mapped.b <= 1.0f);
    ASTER_CHECK(mapped.r > 0.9f);
    const glm::vec3 srgb = LinearToSrgb(glm::vec3(0.5f));
    ASTER_CHECK(srgb.r > 0.5f && srgb.r < 0.8f);
}

ASTER_TEST(RenderMath, ConstantEnvironmentProducesUnitIrradiance)
{
    Image environment;
    environment.Width = 32;
    environment.Height = 16;
    environment.Format = ImageFormat::RGBA32F;
    environment.Data.resize(static_cast<size_t>(environment.Width) * environment.Height * 4 * sizeof(float));
    float* pixels = reinterpret_cast<float*>(environment.Data.data());
    for (size_t index = 0; index < static_cast<size_t>(environment.Width) * environment.Height; ++index)
    {
        pixels[index * 4 + 0] = 1.0f;
        pixels[index * 4 + 1] = 1.0f;
        pixels[index * 4 + 2] = 1.0f;
        pixels[index * 4 + 3] = 1.0f;
    }

    const Sh9 sh = IblBaker::ProjectIrradiance(environment, 2048);
    for (const glm::vec3& normal : { glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
             glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(-0.5f, 0.5f, 0.7f) })
    {
        const glm::vec3 irradiance = sh.Evaluate(glm::normalize(normal));
        ASTER_CHECK_NEAR(irradiance.r, 1.0f, 0.05f);
        ASTER_CHECK_NEAR(irradiance.g, 1.0f, 0.05f);
        ASTER_CHECK_NEAR(irradiance.b, 1.0f, 0.05f);
    }
}

ASTER_TEST(RenderMath, BrdfLutIsSane)
{
    std::vector<glm::vec2> lut;
    IblBaker::BuildBrdfLut(16, lut);
    ASTER_CHECK_EQ(lut.size(), static_cast<size_t>(16 * 16));
    for (const glm::vec2& value : lut)
    {
        ASTER_CHECK(std::isfinite(value.x));
        ASTER_CHECK(std::isfinite(value.y));
        ASTER_CHECK(value.x >= 0.0f && value.x <= 1.0f);
        ASTER_CHECK(value.y >= 0.0f && value.y <= 1.0f);
    }
}

ASTER_TEST(RenderMath, IblBakeProducesMipsAndIrr)
{
    Image environment;
    environment.Width = 8;
    environment.Height = 4;
    environment.Format = ImageFormat::RGBA32F;
    environment.Data.resize(static_cast<size_t>(environment.Width) * environment.Height * 4 * sizeof(float));
    float* pixels = reinterpret_cast<float*>(environment.Data.data());
    for (size_t index = 0; index < static_cast<size_t>(environment.Width) * environment.Height; ++index)
    {
        pixels[index * 4 + 0] = 0.5f;
        pixels[index * 4 + 1] = 0.25f;
        pixels[index * 4 + 2] = 0.125f;
        pixels[index * 4 + 3] = 1.0f;
    }

    Environment baked;
    std::string error;
    ASTER_CHECK(IblBaker::Bake(environment, baked, &error));
    ASTER_CHECK(baked.PrefilteredSpecular.FaceSize == 64);
    ASTER_CHECK(baked.PrefilteredSpecular.MipCount == 6);
    ASTER_CHECK(!baked.PrefilteredSpecular.Pixels.empty());
    ASTER_CHECK(baked.BrdfLutSize == 128);
    ASTER_CHECK_EQ(baked.BrdfLut.size(), static_cast<size_t>(128 * 128));
}

ASTER_TEST(RenderMath, DirectionalShadowMatrixCoversSceneCenter)
{
    const glm::vec3 center(2.0f, 1.5f, -3.0f);
    const DirectionalShadowMatrix shadow = BuildDirectionalShadowMatrix(center, glm::vec3(6.0f, 0.2f, 4.0f),
        glm::vec3(0.5f, -1.0f, 0.25f));
    ASTER_CHECK(shadow.Radius >= 4.0f);

    const glm::vec4 clip = shadow.ViewProjection * glm::vec4(center, 1.0f);
    ASTER_CHECK(clip.w > 0.0f);
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    ASTER_CHECK_NEAR(ndc.x, 0.0f, 1e-3);
    ASTER_CHECK_NEAR(ndc.y, 0.0f, 1e-3);
    ASTER_CHECK(ndc.z >= 0.0f && ndc.z <= 1.0f);

    const DirectionalShadowMatrix tiny = BuildDirectionalShadowMatrix(glm::vec3(0.0f), glm::vec3(0.01f),
        glm::vec3(0.0f, -1.0f, 0.0f));
    ASTER_CHECK_NEAR(tiny.Radius, 4.0f, 1e-5);
}

ASTER_TEST(RenderMath, CameraRayAndProjection)
{
    Camera camera;
    camera.SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    camera.SetViewportSize(1600, 900);
    camera.SetPosition(glm::vec3(0.0f, 2.0f, 5.0f));
    camera.SetRotation(-90.0f, 0.0f);

    const Ray ray = camera.ScreenToWorldRay(glm::vec2(800.0f, 450.0f), glm::vec2(1600.0f, 900.0f));
    const glm::vec3 expected = camera.GetForward();
    ASTER_CHECK_NEAR(glm::dot(ray.Direction, expected), 1.0f, 1e-4);

    glm::vec2 screen;
    ASTER_CHECK(camera.WorldToScreen(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec2(1600.0f, 900.0f), screen));
    ASTER_CHECK_NEAR(screen.x, 800.0f, 1.0f);
    ASTER_CHECK_NEAR(screen.y, 450.0f, 1.0f);
}
