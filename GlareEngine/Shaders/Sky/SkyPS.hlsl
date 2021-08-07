#include "../Misc/CommonResource.hlsli"


TextureCube gCubeMap : register(t0);

//曝光参数
#define exposure 1.0f

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
};


float4 main(VertexOut pin) : SV_Target
{
    float4 litColor = gCubeMap.Sample(gsamLinearWrap, pin.PosL);

	// Reinhard色调映射
	//litColor.rgb = litColor.rgb / (litColor.rgb + float3(1.0f, 1.0f, 1.0f));

	// 曝光色调映射
    litColor.rgb = float3(1.0f, 1.0f, 1.0f) - exp(-litColor.rgb * exposure);

    return litColor;
}