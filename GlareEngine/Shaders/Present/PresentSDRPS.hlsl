//#include "../Misc/ShaderUtility.hlsli"
//#include "../Misc/ToneMappingUtility.hlsli"

Texture2D<float3> ColorTex : register(t0);


float4 main(float4 position : SV_Position) : SV_Target0
{
    //float3 LinearRGB = RemoveDisplayProfile(ColorTex[(int2) position.xy], LDR_COLOR_FORMAT);
    //return float4(ApplyDisplayProfile(LinearRGB, DISPLAY_PLANE_FORMAT), 1.0f);

    return float4(ColorTex[(int2) position.xy], 1.0f);
}