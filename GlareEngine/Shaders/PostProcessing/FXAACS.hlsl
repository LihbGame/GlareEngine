#define FXAA_PC             1
#define FXAA_HLSL_5         1
#define FXAA_GREEN_AS_LUMA  1

//#define FXAA_QUALITY__PRESET 12
//#define FXAA_QUALITY__PRESET 25
#define FXAA_QUALITY__PRESET 39   //Height Quality

#include "../Misc/CommonResource.hlsli"
#include "FXAA.hlsli"

static const float FxaaSubpix               = 0.75;
static const float FxaaEdgeThreshold        = 0.166;
static const float FxaaEdgeThresholdMin     = 0.0833;

Texture2D<float4> Input         : register(t0);
RWTexture2D<float4> Output      : register(u0);

cbuffer FxaaConstBuffer         : register(b0)
{
    float2 g_InverseDimensions;
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    const float2 UV = (DTid.xy + 0.5f) * g_InverseDimensions;

    FxaaTex TexData = { gSamplerLinearClamp, Input };

    Output[DTid.xy] = FxaaPixelShader(UV, 0, TexData, TexData, TexData, g_InverseDimensions, 0, 0, 0, FxaaSubpix, FxaaEdgeThreshold, FxaaEdgeThresholdMin, 0, 0, 0, 0);
}