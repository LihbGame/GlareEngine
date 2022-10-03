#include "../Misc/CommonResource.hlsli"

Texture2D<float3> ColorTex : register(t0);


float4 main(float4 position : SV_Position) : SV_Target0
{
    return float4(FBM_Liquid(position.xy), 1.0f);
}