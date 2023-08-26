#include "../Misc/CommonResource.hlsli"


struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
    nointerpolation uint MatIndex : MATINDEX;
};

//This is only used for alpha clipping geometry so that shadows show up correctly. 
void main(VertexOut pin)
{
	// Fetch the material data.
    MaterialData matData = gMaterialData[pin.MatIndex];
    uint diffuseMapIndex = matData.mDiffuseMapIndex;
	// Dynamically look up the texture in the array.
    float a = gSRVMap[diffuseMapIndex].Sample(gSamplerAnisoWrap, pin.TexC).a;

    clip(a - 0.1f);
}