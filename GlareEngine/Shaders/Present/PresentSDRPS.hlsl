#include "ShaderUtility.hlsli"

Texture2D<float3> ColorTex : register(t0);

float3 main(float4 position : SV_Position) : SV_Target0
{
    float3 LinearRGB = RemoveDisplayProfile(ColorTex[(int2) position.xy], LDR_COLOR_FORMAT);
    return ApplyDisplayProfile(LinearRGB, DISPLAY_PLANE_FORMAT);
}