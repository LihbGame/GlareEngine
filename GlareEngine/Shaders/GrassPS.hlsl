#include "Common.hlsli"

struct GSOutput
{
	float4 pos : SV_POSITION;
	float2 Tex  : TEXCOORD;
};



float4 PS(GSOutput pin) : SV_TARGET
{
	// Fetch the material data.
	MaterialData matData = gMaterialData[gMaterialIndex];
	float4 diffuseAlbedo = gSRVMap[matData.DiffuseMapIndex].Sample(gsamLinearWrap, pin.Tex);
	clip(diffuseAlbedo.a - 0.1f);
	return diffuseAlbedo;
}