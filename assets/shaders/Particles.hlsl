cbuffer FrameConstants : register(b0)
{
    float4x4 ViewProjection;
};

struct VSInput
{
    float3 Position : POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
    float2 UV : TEXCOORD;
};

VSOutput VSParticles(VSInput input)
{
    VSOutput output;
    output.Position = mul(ViewProjection, float4(input.Position, 1.0f));
    output.Color = input.Color;
    output.UV = input.UV;
    return output;
}

float4 PSParticles(VSOutput input) : SV_Target
{
    const float2 centered = input.UV * 2.0f - 1.0f;
    const float radiusSquared = dot(centered, centered);
    if (radiusSquared > 1.0f)
    {
        discard;
    }

    // Soft round sprite without requiring a texture; the bright HDR color feeds bloom.
    const float falloff = saturate(1.0f - radiusSquared);
    const float alpha = falloff * falloff * input.Color.a;
    return float4(input.Color.rgb, alpha);
}
