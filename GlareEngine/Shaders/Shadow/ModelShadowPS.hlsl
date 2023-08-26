#include "../Misc/CommonResource.hlsli"


struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
    nointerpolation uint MatIndex : MATINDEX;
};

//�������alpha���м��Σ��Ա���Ӱ��ȷ��ʾ�� 
//����Ҫ��������в����ļ��������ʹ��NULL������ɫ��������ȴ��ݡ�
void main(VertexOut pin)
{
	// Fetch the material data.
    MaterialData matData = gMaterialData[pin.MatIndex];
    uint diffuseMapIndex = matData.mDiffuseMapIndex;
	// Dynamically look up the texture in the array.
    float a = gSRVMap[diffuseMapIndex].Sample(gSamplerAnisoWrap, pin.TexC).a;

    clip(a - 0.1f);
}