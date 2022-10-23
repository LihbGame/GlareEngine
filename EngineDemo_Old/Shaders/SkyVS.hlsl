#include "Common.hlsli"


struct VertexOut
{
	float4 PosH : SV_POSITION;
	float3 PosL : POSITION;
};



VertexOut VS(float3 PosL : POSITION )
{
	VertexOut vout;

	// ʹ�þֲ�����λ����Ϊ��������ͼ����������
	vout.PosL = PosL;

	// ת��������ռ�
	float4 posW = mul(float4(PosL, 1.0f), gWorld);

	//����պй̶����۾�λ��
	posW.xyz += gEyePosW;

	// ����z = w����ʹz / w = 1���������ʼ��λ��Զƽ���ϣ���
	vout.PosH = mul(posW, gViewProj).xyww;

	return vout;
}


float4 PS(VertexOut pin) : SV_Target
{
return gCubeMap.Sample(gsamLinearWrap, pin.PosL);
}