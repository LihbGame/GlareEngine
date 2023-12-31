#include "../Misc/CommonResource.hlsli"

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TexCoord0;
};

Texture2D<float4> baseColorTexture          : register(t20);
SamplerState baseColorSampler               : register(s0);

cbuffer MaterialConstants : register(b2)
{
    float4 baseColorFactor;
    float3 emissiveFactor;
    float normalTextureScale;
    float2 metallicRoughnessFactor;
    uint flags;
}

void main(VSOutput vsOutput)
{
    //float cutoff = f16tof32(flags >> 16);
    if (baseColorTexture.Sample(baseColorSampler, vsOutput.uv).a <= 0.5f)
        discard;
}