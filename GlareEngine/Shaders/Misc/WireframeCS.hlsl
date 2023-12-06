#include "../Shadow/RealTimeShadowHelper.hlsli"

Texture2D<float3> GBUFFER_Emissive : register(t4);

RWTexture2D<float3> OutColor : register(u0);

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint2 pixelPos = DTid.xy;
    OutColor[pixelPos] = GBUFFER_Emissive[pixelPos];
}