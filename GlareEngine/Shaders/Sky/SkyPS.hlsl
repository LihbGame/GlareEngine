#include "../Misc/CommonResource.hlsli"


//�ع����
#define exposure 1.0f


float4 main(PosVSOut pin) : SV_Target
{
    float4 litColor = gCubeMaps[gSkyCubeIndex].Sample(gSamplerLinearWrap, pin.PosL);

	// Reinhardɫ��ӳ��
	//litColor.rgb = litColor.rgb / (litColor.rgb + float3(1.0f, 1.0f, 1.0f));

	// �ع�ɫ��ӳ��
    //litColor.rgb = float3(1.0f, 1.0f, 1.0f) - exp(-litColor.rgb * exposure);

    return litColor;
}