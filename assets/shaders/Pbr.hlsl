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
    const float4 hdr = HdrTexture.Sample(HdrSampler, input.UV);
    const float3 mapped = ACESFilm(hdr.rgb * max(Exposure, 0.0001f));
    return float4(mapped, 1.0f);
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
    color += ambient;
    color += Emissive.rgb * Material.z;

    return float4(color, saturate(surface.a * BaseColor.a * Material.w));
}
