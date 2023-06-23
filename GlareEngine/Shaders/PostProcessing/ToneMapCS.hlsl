#include "../Misc/ToneMappingUtility.hlsli"

StructuredBuffer<float> Exposure : register(t0);
Texture2D<float3> Bloom : register(t1);

#if SUPPORT_TYPED_UAV_LOADS
RWTexture2D<float3> ColorRW : register(u0);
#else //not support R11G11B10 UAV RW
RWTexture2D<uint> DstColor : register(u0);
Texture2D<float3> SrcColor : register(t2);
#endif

RWTexture2D<float> OutLuma : register(u1);
SamplerState LinearSampler : register(s0);


cbuffer ConstantBuffer : register(b0)
{
    float2 g_RcpBufferDimensions;
    float g_BloomStrength;
    float g_PaperWhiteRatio; // PaperWhite / MaxBrightness
    float g_MaxBrightness;
};


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

#if ENABLE_HDR_DISPLAY_MAPPING
    HDRColor = TM_Stanard(REC709toREC2020(HDRColor) * g_PaperWhiteRatio) * g_MaxBrightness;
    //Apply REC2084 Curve ,10000 is a scale for HDR color
    HDRColor = ApplyREC2084Curve(HDRColor / 10000);

#if SUPPORT_TYPED_UAV_LOADS
    ColorRW[DTid.xy] = HDRColor;
#else
    DstColor[DTid.xy] = Pack_R11G11B10_FLOAT(HDRColor);
#endif
    OutLuma[DTid.xy] = RGBToLogLuminance(HDRColor);

#else 
    // Tone map to SDR
    float3 SDRColor = TM_Stanard(HDRColor);

    //LDR Color Correct
    SDRColor = ApplyDisplayProfile(SDRColor, DISPLAY_PLANE_FORMAT);

#if SUPPORT_TYPED_UAV_LOADS
    ColorRW[DTid.xy] = SDRColor;
#else
    DstColor[DTid.xy] = Pack_R11G11B10_FLOAT(SDRColor);
#endif

    OutLuma[DTid.xy] = RGBToLogLuminance(SDRColor);

#endif //ENABLE_HDR_DISPLAY_MAPPING
}