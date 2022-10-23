#include "Common.hlsli"

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
	nointerpolation uint MatIndex  : MATINDEX;
};



//�������alpha���м��Σ��Ա���Ӱ��ȷ��ʾ�� 
//����Ҫ��������в����ļ��������ʹ��NULL������ɫ��������ȴ��ݡ�
void PS(VertexOut pin)
{
	// Fetch the material data.
	MaterialData matData = gMaterialData[pin.MatIndex];
	uint diffuseMapIndex = matData.DiffuseMapIndex;

	// Dynamically look up the texture in the array.
	float a = gSRVMap[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC).a;

#ifdef ALPHA_TEST
	clip(a - 0.1f);
#endif
}