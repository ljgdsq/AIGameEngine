cbuffer FrameConstants : register(b0)
{
    float4x4 ViewProjection;
    float4 CameraPosition;
    float4 LightDirection;
    float4 LightColor;
    float4 AmbientColor;
    float4 PointLightPosition[4];
    float4 PointLightColor[4];
    float4 SpotLightPosition[4];
    float4 SpotLightDirection[4]; // xyz direction, w cos(outer angle)
    float4 SpotLightColor[4];     // rgb color, w cos(inner angle)
    int PointLightCount;
    int SpotLightCount;
    float Exposure;
    float Time;
    float4x4 LightViewProjection;
    float4 ShadowParams; // x depth bias, y texel size, z shadow radius, w enabled
    float4x4 InverseViewProjection;
    float4 EnvironmentParams; // x enabled, y intensity, z max mip, w show skybox
    float4 IrradianceSh[9];
    float4 BloomParams; // x enabled, y threshold, z intensity, w unused
    float4 ScreenParams; // width, height, 1/width, 1/height
    float4 SsaoParams; // x radius, y intensity, z bias, w enabled
};

cbuffer ObjectConstants : register(b1)
{
    float4x4 Model;
    float4x4 NormalMatrix;
    float4 BaseColor;
    float4 Material; // metallic, roughness, emissive strength, opacity
    float4 Emissive;
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
    float4 Tangent : TANGENT;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float3 WorldPosition : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 UV : TEXCOORD2;
    float3 Tangent : TEXCOORD3;
    float3 Bitangent : TEXCOORD4;
};

Texture2D AlbedoTexture : register(t0);
SamplerState AlbedoSampler : register(s0);
Texture2D ShadowMap : register(t1);
SamplerState ShadowSampler : register(s1);
Texture2D HdrTexture : register(t2);
SamplerState HdrSampler : register(s2);
TextureCube EnvironmentMap : register(t3);
SamplerState EnvironmentSampler : register(s3);
Texture2D BrdfLut : register(t4);
SamplerState BrdfSampler : register(s4);
Texture2D<float> DepthTexture : register(t5);
SamplerState DepthSampler : register(s5);
Texture2D<float> AoTexture : register(t6);
SamplerState AoSampler : register(s6);

struct PostVSOutput
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

float4 VSShadow(VSInput input) : SV_Position
{
    const float4 worldPosition = mul(Model, float4(input.Position, 1.0f));
    return mul(LightViewProjection, worldPosition);
}

float4 PSShadow(float4 position : SV_Position) : SV_Target
{
    return 0.0f.xxxx;
}

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    const float4 worldPosition = mul(Model, float4(input.Position, 1.0f));
    output.WorldPosition = worldPosition.xyz;
    output.Position = mul(ViewProjection, worldPosition);
    output.Normal = normalize(mul((float3x3)NormalMatrix, input.Normal));
    output.Tangent = normalize(mul((float3x3)NormalMatrix, input.Tangent.xyz));
    output.Bitangent = cross(output.Normal, output.Tangent) * input.Tangent.w;
    output.UV = input.UV;
    return output;
}

static const float PI = 3.14159265359f;

float DistributionGGX(float3 normal, float3 halfway, float roughness)
{
    const float a = roughness * roughness;
    const float a2 = a * a;
    const float nDotH = saturate(dot(normal, halfway));
    const float nDotH2 = nDotH * nDotH;
    const float denominator = nDotH2 * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * denominator * denominator, 1e-5f);
}

float GeometrySchlickGGX(float nDotV, float roughness)
{
    const float r = roughness + 1.0f;
    const float k = (r * r) / 8.0f;
    return nDotV / max(nDotV * (1.0f - k) + k, 1e-5f);
}

float GeometrySmith(float3 normal, float3 view, float3 light, float roughness)
{
    return GeometrySchlickGGX(saturate(dot(normal, view)), roughness) *
        GeometrySchlickGGX(saturate(dot(normal, light)), roughness);
}

float3 FresnelSchlick(float cosTheta, float3 f0)
{
    return f0 + (1.0f - f0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 f0, float roughness)
{
    const float3 maximum = max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), f0);
    return f0 + (maximum - f0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float3 ACESFilm(float3 value)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((value * (a * value + b)) / (value * (c * value + d) + e));
}

PostVSOutput VSFullscreen(uint vertexID : SV_VertexID)
{
    PostVSOutput output;
    const float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.Position = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.0f, 1.0f);
    output.UV = uv;
    return output;
}

float4 PSTonemap(PostVSOutput input) : SV_Target
{
    const float2 texel = ScreenParams.zw;
    const float aoCenter = AoTexture.Sample(AoSampler, input.UV).r;
    const float ao = aoCenter * 0.4f +
        (AoTexture.Sample(AoSampler, input.UV + float2(texel.x, 0.0f)).r +
         AoTexture.Sample(AoSampler, input.UV - float2(texel.x, 0.0f)).r +
         AoTexture.Sample(AoSampler, input.UV + float2(0.0f, texel.y)).r +
         AoTexture.Sample(AoSampler, input.UV - float2(0.0f, texel.y)).r) * 0.15f;
    float3 hdr = HdrTexture.Sample(HdrSampler, input.UV).rgb * ao;
    if (BloomParams.x > 0.5f)
    {
        const float2 bloomTexel = texel * 1.5f;
        float3 bloom = 0.0f;
        [unroll] for (int y = -2; y <= 2; ++y)
        {
            [unroll] for (int x = -2; x <= 2; ++x)
            {
                const float3 sampleColor = HdrTexture.SampleLevel(HdrSampler,
                    input.UV + float2(x, y) * bloomTexel, 0.0f).rgb;
                bloom += max(sampleColor - BloomParams.y, 0.0f);
            }
        }
        hdr += bloom * (BloomParams.z / 25.0f);
    }
    const float3 mapped = ACESFilm(hdr * max(Exposure, 0.0001f));
    return float4(mapped, 1.0f);
}

float3 ReconstructWorldPosition(float2 uv, float depth)
{
    const float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    const float4 clip = float4(ndc, depth, 1.0f);
    const float4 world = mul(InverseViewProjection, clip);
    return world.xyz / max(abs(world.w), 1e-6f) * sign(world.w);
}

// Screen-space ambient occlusion from the hardware depth buffer. Normals are derived from
// reconstructed world positions so the pass needs no geometry buffer. The result is a single
// channel mask consumed by PSTonemap.
float4 PSSsao(PostVSOutput input) : SV_Target
{
    if (SsaoParams.w < 0.5f)
    {
        return 1.0f.xxxx;
    }

    const float depth = DepthTexture.SampleLevel(DepthSampler, input.UV, 0.0f).r;
    if (depth >= 0.99999f)
    {
        return 1.0f.xxxx;
    }

    const float3 worldPosition = ReconstructWorldPosition(input.UV, depth);
    float3 normal = normalize(cross(ddy(worldPosition), ddx(worldPosition)));
    if (dot(normal, CameraPosition.xyz - worldPosition) < 0.0f)
    {
        normal = -normal;
    }

    float3 tangent = normalize(abs(normal.y) < 0.95f
        ? cross(normal, float3(0.0f, 1.0f, 0.0f))
        : cross(normal, float3(1.0f, 0.0f, 0.0f)));
    float3 bitangent = cross(normal, tangent);
    const float angle = frac(sin(dot(input.Position.xy, float2(12.9898f, 78.233f))) * 43758.5453f) *
        6.2831853f;
    const float3 rotatedTangent = tangent * cos(angle) + bitangent * sin(angle);
    bitangent = cross(normal, rotatedTangent);
    tangent = rotatedTangent;

    static const float3 kernel[12] = {
        float3(0.06f, 0.02f, 0.03f), float3(-0.05f, 0.03f, 0.05f),
        float3(0.02f, -0.06f, 0.04f), float3(-0.02f, -0.03f, 0.07f),
        float3(0.08f, 0.05f, 0.09f), float3(-0.08f, -0.05f, 0.06f),
        float3(0.04f, 0.08f, 0.10f), float3(-0.04f, 0.07f, 0.03f),
        float3(0.02f, -0.09f, 0.08f), float3(-0.09f, 0.01f, 0.04f),
        float3(0.07f, -0.04f, 0.05f), float3(-0.03f, -0.08f, 0.09f)
    };

    const float radius = max(SsaoParams.x, 0.01f);
    const float bias = max(SsaoParams.z, 0.001f);
    float occlusion = 0.0f;
    [unroll] for (int index = 0; index < 12; ++index)
    {
        const float3 samplePosition = worldPosition +
            (tangent * kernel[index].x + bitangent * kernel[index].y + normal * kernel[index].z) * radius;
        const float4 sampleClip = mul(ViewProjection, float4(samplePosition, 1.0f));
        if (sampleClip.w <= 0.0001f)
        {
            continue;
        }
        const float2 sampleUv = sampleClip.xy / sampleClip.w * 0.5f + 0.5f;
        if (sampleUv.x < 0.0f || sampleUv.x > 1.0f || sampleUv.y < 0.0f || sampleUv.y > 1.0f)
        {
            continue;
        }
        const float sceneDepth = DepthTexture.SampleLevel(DepthSampler, sampleUv, 0.0f).r;
        if (sceneDepth >= 0.99999f)
        {
            continue;
        }
        const float3 scenePosition = ReconstructWorldPosition(sampleUv, sceneDepth);
        const float sceneDistance = distance(scenePosition, CameraPosition.xyz);
        const float sampleDistance = distance(samplePosition, CameraPosition.xyz);
        const float rangeCheck = saturate(radius / max(distance(scenePosition, worldPosition), 0.001f));
        occlusion += (sceneDistance < sampleDistance - bias ? 1.0f : 0.0f) * rangeCheck;
    }

    const float ao = saturate(1.0f - (occlusion / 12.0f) * max(SsaoParams.y, 0.0f));
    return float4(ao.xxx, 1.0f);
}

float4 PSSky(PostVSOutput input) : SV_Target
{
    const float2 ndc = float2(input.UV.x * 2.0f - 1.0f, 1.0f - input.UV.y * 2.0f);
    const float4 nearClip = mul(InverseViewProjection, float4(ndc, 0.0f, 1.0f));
    const float4 farClip = mul(InverseViewProjection, float4(ndc, 1.0f, 1.0f));
    const float3 nearWorld = nearClip.xyz / max(abs(nearClip.w), 1e-5f) * sign(nearClip.w);
    const float3 farWorld = farClip.xyz / max(abs(farClip.w), 1e-5f) * sign(farClip.w);
    const float3 direction = normalize(farWorld - nearWorld);
    const float3 sky = EnvironmentMap.SampleLevel(EnvironmentSampler, direction, 0.0f).rgb *
        max(EnvironmentParams.y, 0.0f);
    return float4(max(sky, 0.0f.xxx), 1.0f);
}

float3 EvaluateLight(float3 normal, float3 view, float3 lightDirection, float3 radiance, float3 albedo,
    float metallic, float roughness, float3 f0)
{
    const float3 halfway = normalize(view + lightDirection);
    const float nDotL = saturate(dot(normal, lightDirection));
    if (nDotL <= 0.0f)
    {
        return 0.0f;
    }

    const float ndf = DistributionGGX(normal, halfway, roughness);
    const float geometry = GeometrySmith(normal, view, lightDirection, roughness);
    const float3 fresnel = FresnelSchlick(saturate(dot(halfway, view)), f0);
    const float3 specular = (ndf * geometry * fresnel) /
        max(4.0f * saturate(dot(normal, view)) * nDotL, 1e-4f);
    const float3 diffuse = (1.0f - fresnel) * (1.0f - metallic) * albedo / PI;
    return (diffuse + specular) * radiance * nDotL;
}

float SampleShadow(float3 worldPosition, float3 normal, float3 lightDirection)
{
    if (ShadowParams.w < 0.5f)
    {
        return 1.0f;
    }

    const float4 lightClip = mul(LightViewProjection, float4(worldPosition, 1.0f));
    const float3 projected = lightClip.xyz / max(lightClip.w, 1e-5f);
    const float2 uv = float2(projected.x * 0.5f + 0.5f, 0.5f - projected.y * 0.5f);
    if (any(uv < 0.0f) || any(uv > 1.0f) || projected.z < 0.0f || projected.z > 1.0f)
    {
        return 1.0f;
    }

    const float nDotL = saturate(dot(normal, lightDirection));
    const float bias = max(ShadowParams.x * (1.0f - nDotL), 0.0005f);
    float visibility = 0.0f;
    [unroll] for (int y = -1; y <= 1; ++y)
    {
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            const float2 offset = float2(x, y) * ShadowParams.y;
            const float sampleDepth = ShadowMap.SampleLevel(ShadowSampler, uv + offset, 0).r;
            visibility += (projected.z - bias <= sampleDepth) ? 1.0f : 0.0f;
        }
    }
    return visibility / 9.0f;
}

float3 EvaluateIrradianceSh(float3 normal)
{
    const float x = normal.x;
    const float y = normal.y;
    const float z = normal.z;
    float3 result = IrradianceSh[0].xyz * 0.28209479177387814f;
    result += IrradianceSh[1].xyz * (0.4886025119029199f * y);
    result += IrradianceSh[2].xyz * (0.4886025119029199f * z);
    result += IrradianceSh[3].xyz * (0.4886025119029199f * x);
    result += IrradianceSh[4].xyz * (1.0925484305920792f * x * y);
    result += IrradianceSh[5].xyz * (1.0925484305920792f * y * z);
    result += IrradianceSh[6].xyz * (0.31539156525252005f * (3.0f * z * z - 1.0f));
    result += IrradianceSh[7].xyz * (1.0925484305920792f * x * z);
    result += IrradianceSh[8].xyz * (0.5462742152960396f * (x * x - y * y));
    return max(result, 0.0f.xxx);
}

float3 SampleEnvironment(float3 direction, float roughness)
{
    const float mip = saturate(roughness) * max(EnvironmentParams.z, 0.0f);
    return max(EnvironmentMap.SampleLevel(EnvironmentSampler, direction, mip).rgb, 0.0f.xxx);
}

float3 EvaluateEnvironment(float3 normal, float3 view, float3 albedo, float metallic, float roughness, float3 f0)
{
    const float nDotV = saturate(dot(normal, view));
    const float3 reflection = reflect(-view, normal);
    const float3 prefiltered = SampleEnvironment(reflection, roughness);
    const float2 brdf = BrdfLut.Sample(BrdfSampler, float2(nDotV, roughness)).rg;
    const float3 fresnel = FresnelSchlickRoughness(nDotV, f0, roughness);
    const float3 specular = prefiltered * (fresnel * brdf.x + brdf.y);
    const float3 irradiance = EvaluateIrradianceSh(normal);
    const float3 diffuse = irradiance * albedo * (1.0f - fresnel) * (1.0f - metallic);
    return (diffuse + specular) * max(EnvironmentParams.y, 0.0f);
}

float4 PSMain(VSOutput input) : SV_Target
{
    const float3 normal = normalize(input.Normal);
    const float3 view = normalize(CameraPosition.xyz - input.WorldPosition);
    const float4 surface = AlbedoTexture.Sample(AlbedoSampler, input.UV);
    // Project textures are authored in sRGB. The swapchain and lighting are linear.
    const float3 albedo = BaseColor.rgb * pow(max(surface.rgb, 1e-4f), 2.2f);
    const float metallic = saturate(Material.x);
    const float roughness = clamp(Material.y, 0.04f, 1.0f);
    const float3 f0 = lerp(0.04f.xxx, albedo, metallic);

    float3 color = 0.0f.xxx;

    // Main directional light. LightDirection points away from the light source.
    const float3 lightDirection = normalize(-LightDirection.xyz);
    const float shadow = SampleShadow(input.WorldPosition, normal, lightDirection);
    color += EvaluateLight(normal, view, lightDirection, LightColor.rgb, albedo, metallic, roughness, f0) *
        shadow;

    for (int index = 0; index < PointLightCount && index < 4; ++index)
    {
        const float3 toLight = PointLightPosition[index].xyz - input.WorldPosition;
        const float distance = length(toLight);
        const float range = max(PointLightColor[index].w, 0.001f);
        const float attenuation = saturate(1.0f - distance / range);
        const float3 radiance = PointLightColor[index].rgb * attenuation * attenuation;
        color += EvaluateLight(normal, view, toLight / max(distance, 1e-4f), radiance, albedo, metallic,
            roughness, f0);
    }

    for (int index = 0; index < SpotLightCount && index < 4; ++index)
    {
        const float3 toLight = SpotLightPosition[index].xyz - input.WorldPosition;
        const float distance = length(toLight);
        const float range = max(SpotLightPosition[index].w, 0.001f);
        if (distance >= range)
        {
            continue;
        }

        const float3 lightDirection = toLight / max(distance, 1e-4f);
        const float cosAngle = dot(normalize(SpotLightDirection[index].xyz), -lightDirection);
        const float cone = smoothstep(SpotLightDirection[index].w, SpotLightColor[index].w, cosAngle);
        const float attenuation = saturate(1.0f - distance / range);
        const float3 radiance = SpotLightColor[index].rgb * cone * attenuation * attenuation;
        color += EvaluateLight(normal, view, lightDirection, radiance, albedo, metallic, roughness, f0);
    }

    const float hemisphere = saturate(normal.y * 0.5f + 0.5f);
    const float3 ambient = AmbientColor.rgb * albedo * (0.35f + 0.65f * hemisphere);
    if (EnvironmentParams.x > 0.5f)
    {
        color += EvaluateEnvironment(normal, view, albedo, metallic, roughness, f0);
    }
    else
    {
        color += ambient;
    }
    color += Emissive.rgb * Material.z;

    return float4(color, saturate(surface.a * BaseColor.a * Material.w));
}
