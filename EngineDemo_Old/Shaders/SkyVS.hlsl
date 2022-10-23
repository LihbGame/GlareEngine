#include "Common.hlsli"


struct VertexOut
{
	float4 PosH : SV_POSITION;
	float3 PosL : POSITION;
};



VertexOut VS(float3 PosL : POSITION )
{
	VertexOut vout;

	// 使用局部顶点位置作为立方体贴图采样向量。
	vout.PosL = PosL;

	// 转换到世界空间
	float4 posW = mul(float4(PosL, 1.0f), gWorld);

	//把天空盒固定到眼睛位置
	posW.xyz += gEyePosW;

	// 设置z = w，以使z / w = 1（即，天穹始终位于远平面上）。
	vout.PosH = mul(posW, gViewProj).xyww;

	return vout;
}


float4 PS(VertexOut pin) : SV_Target
{
return gCubeMap.Sample(gsamLinearWrap, pin.PosL);
}