#include "Common.hlsli"

//曝光参数
#define exposure 1.0f

struct VertexOut
{
	float4 PosH : SV_POSITION;
	float3 PosL : POSITION;
};


float4 PS(VertexOut pin) : SV_Target
{
	float4 litColor= gCubeMap.Sample(gsamLinearWrap, pin.PosL);

	// Reinhard色调映射
	//litColor.rgb = litColor.rgb / (litColor.rgb + float3(1.0f, 1.0f, 1.0f));

	// 曝光色调映射
	litColor.rgb = float3(1.0f,1.0f,1.0f) - exp(-litColor.rgb * exposure);
	//Gamma
	float3 gamma = float(1.0 / 2.2);
	litColor.rgb = pow(litColor.rgb, gamma);
	
	return litColor;
}