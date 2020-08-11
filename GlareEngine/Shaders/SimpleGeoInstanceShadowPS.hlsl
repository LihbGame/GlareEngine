#include "Common.hlsli"

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
};



//这仅用于alpha剪切几何，以便阴影正确显示。 
//不需要对纹理进行采样的几何体可以使用NULL像素着色器进行深度传递。
void PS(VertexOut pin)
{
	// Fetch the material data.
	MaterialData matData = gMaterialData[gMaterialIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
	uint diffuseMapIndex = matData.DiffuseMapIndex;

	// Dynamically look up the texture in the array.
	diffuseAlbedo *= gSRVMap[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);

#ifdef ALPHA_TEST
	// Discard pixel if texture alpha < 0.1.  We do this test as soon 
	// as possible in the shader so that we can potentially exit the
	// shader early, thereby skipping the rest of the shader code.
	clip(diffuseAlbedo.a - 0.1f);
#endif
}