#include "../Misc/ShaderUtility.hlsli"
#include "../Misc/ToneMappingUtility.hlsli"

Texture2D<float3> ColorTex : register(t0);


float3 main(float4 position : SV_Position) : SV_Target0
{
    //50 is a scale for HDR color
    return ApplyREC2084Curve(ColorTex[(int2)position.xy] / 50.0);
}