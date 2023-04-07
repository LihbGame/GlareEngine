#include "../Misc/ToneMappingUtility.hlsli"


StructuredBuffer<float> Exposure : register(t0);
Texture2D<float3> Bloom : register(t1);

#if SUPPORT_TYPED_UAV_LOADS
RWTexture2D<float3> ColorRW : register(u0);
#else
RWTexture2D<uint> DstColor : register(u0);
Texture2D<float3> SrcColor : register(t2);
#endif

RWTexture2D<float> OutLuma : register(u1);
SamplerState LinearSampler : register(s0);


cbuffer ConstantBuffer : register(b0)
{
    float2 g_RcpBufferDimensions;
    float g_BloomStrength;
    float PaperWhiteRatio; // PaperWhite / MaxBrightness
    float MaxBrightness;
};


// The standard 32-bit HDR color format.  Each float has a 5-bit exponent and no sign bit.
uint Pack_R11G11B10_FLOAT(float3 rgb)
{
    rgb = min(rgb, asfloat(0x477C0000));
    uint r = ((f32tof16(rgb.x) + 8) >> 4) & 0x000007FF;
    uint g = ((f32tof16(rgb.y) + 8) << 7) & 0x003FF800;
    uint b = ((f32tof16(rgb.z) + 16) << 17) & 0xFFC00000;
    return r | g | b;
}

float3 Unpack_R11G11B10_FLOAT(uint rgb)
{
    float r = f16tof32((rgb << 4) & 0x7FF0);
    float g = f16tof32((rgb >> 7) & 0x7FF0);
    float b = f16tof32((rgb >> 17) & 0x7FE0);
    return float3(r, g, b);
}


[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    float2 Coord = (DTid.xy + 0.5) * g_RcpBufferDimensions;

    // Load HDR and bloom
#if SUPPORT_TYPED_UAV_LOADS
    float3 HDRColor = ColorRW[DTid.xy];
#else
    float3 HDRColor = SrcColor[DTid.xy];
#endif

    HDRColor += g_BloomStrength * Bloom.SampleLevel(LinearSampler, Coord, 0);
    HDRColor *= Exposure[0];


}