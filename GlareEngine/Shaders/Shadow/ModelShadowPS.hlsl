#include "../Misc/CommonResource.hlsli"


struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
    nointerpolation uint MatIndex : MATINDEX;
};

//这仅用于alpha剪切几何，以便阴影正确显示。 
//不需要对纹理进行采样的几何体可以使用NULL像素着色器进行深度传递。
void main(VertexOut pin)
{
	// Fetch the material data.
    MaterialData matData = gMaterialData[pin.MatIndex];
    uint diffuseMapIndex = matData.mDiffuseMapIndex;
	// Dynamically look up the texture in the array.
    float a = gSRVMap[diffuseMapIndex].Sample(gSamplerAnisoWrap, pin.TexC).a;

    clip(a - 0.1f);
}