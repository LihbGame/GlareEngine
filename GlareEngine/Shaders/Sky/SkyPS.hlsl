#include "../Misc/CommonResource.hlsli"


//曝光参数
#define exposure 1.0f


float4 main(PosVSOut pin) : SV_Target
{
    float4 litColor = gCubeMaps[gSkyCubeIndex].Sample(gSamplerLinearWrap, pin.PosL);

	// Reinhard色调映射
	//litColor.rgb = litColor.rgb / (litColor.rgb + float3(1.0f, 1.0f, 1.0f));

	// 曝光色调映射
    litColor.rgb = float3(1.0f, 1.0f, 1.0f) - exp(-litColor.rgb * exposure);

    return litColor;
}