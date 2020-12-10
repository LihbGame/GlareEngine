#include "Common.hlsli"
#include "TerrainConstBuffer.hlsli"
float4 PS(DomainOut pin) : SV_TARGET
{
	//
	// Estimate normal and tangent using central differences.
	//
	float2 leftTex = pin.Tex + float2(-gTexelCellSpaceU, 0.0f);
	float2 rightTex = pin.Tex + float2(gTexelCellSpaceU, 0.0f);
	float2 bottomTex = pin.Tex + float2(0.0f, gTexelCellSpaceV);
	float2 topTex = pin.Tex + float2(0.0f, -gTexelCellSpaceV);
	
	float leftY = gSRVMap[mHeightMapIndex].SampleLevel(gsamLinearWrap, leftTex, 0).r;
	float rightY = gSRVMap[mHeightMapIndex].SampleLevel(gsamLinearWrap, rightTex, 0).r;
	float bottomY = gSRVMap[mHeightMapIndex].SampleLevel(gsamLinearWrap, bottomTex, 0).r;
	float topY = gSRVMap[mHeightMapIndex].SampleLevel(gsamLinearWrap, topTex, 0).r;
	
	float3 tangent = normalize(float3(2.0f * gWorldCellSpace, rightY - leftY, 0.0f));
	float3 bitan = normalize(float3(0.0f, bottomY - topY, -2.0f * gWorldCellSpace));
	float3 normalW = cross(tangent, bitan);


	// The toEye vector is used in lighting.
	float3 toEye = gEyePosW - pin.PosW;
	// Cache the distance to the eye from this surface point.
	float distToEye = length(toEye);
	// Normalize.
	toEye /= distToEye;


	//
	// Texturing
	//

	// Sample layers in texture array.
	MaterialData Mat = gMaterialData[gMaterialIndex];
	uint  SrvIndex = Mat.DiffuseMapIndex;
	float4 c[5];
	for (int i = 0; i < 5; ++i)
	{
		c[i] = gSRVMap[SrvIndex].Sample(gsamLinearWrap, pin.TiledTex);
		SrvIndex += 6;
	}

	// Sample the blend map.
	float4 t = gSRVMap[mBlendMapIndex].Sample(gsamLinearWrap, pin.Tex);

	// Blend the layers on top of each other.
	float4 texColor = c[0];
	texColor = lerp(texColor, c[1], t.r);
	texColor = lerp(texColor, c[2], t.g);
	texColor = lerp(texColor, c[3], t.b);
	texColor = lerp(texColor, c[4], t.a);

	return texColor;
}