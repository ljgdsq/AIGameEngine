cbuffer FrameConstants : register(b0)
{
    float4x4 ViewProjection;
};

struct VSInput
{
    float3 Position : POSITION;
    float4 Color : COLOR;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
};

VSOutput VSLine(VSInput input)
{
    VSOutput output;
    output.Position = mul(ViewProjection, float4(input.Position, 1.0f));
    output.Color = input.Color;
    return output;
}

float4 PSLine(VSOutput input) : SV_Target
{
    return input.Color;
}
