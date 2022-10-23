#include "Common.hlsli"

//�ع����
#define exposure 1.0f

struct VertexOut
{
	float4 PosH : SV_POSITION;
	float3 PosL : POSITION;
};


float4 PS(VertexOut pin) : SV_Target
{
	//
	// Fogging
	//
	if (gFogEnabled)
	{
	 return gFogColor;
	}

		float4 litColor = gCubeMap.Sample(gsamLinearWrap, pin.PosL);

		// Reinhardɫ��ӳ��
		//litColor.rgb = litColor.rgb / (litColor.rgb + float3(1.0f, 1.0f, 1.0f));

		// �ع�ɫ��ӳ��
		litColor.rgb = float3(1.0f,1.0f,1.0f) - exp(-litColor.rgb * exposure);
		//Gamma
		float3 gamma = float(1.0 / 2.2);
		litColor.rgb = pow(litColor.rgb, gamma);

		return litColor;
}