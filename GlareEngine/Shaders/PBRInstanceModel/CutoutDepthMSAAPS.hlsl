#include "../Misc/CommonResource.hlsli"

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TexCoord0;
};

Texture2D<float4> baseColorTexture          : register(t10);
SamplerState baseColorSampler               : register(s0);

cbuffer MaterialConstants : register(b2)
{
    float4 baseColorFactor;
    float3 emissiveFactor;
    float normalTextureScale;
    float2 metallicRoughnessFactor;
    uint flags;
}

float4 main(VSOutput vsOutput) :SV_Target
{
    return baseColorTexture.Sample(baseColorSampler, vsOutput.uv);
}