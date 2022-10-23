#include "Common.hlsli"
#include "TerrainConstBuffer.hlsli"

struct DomainOut
{
	float4 PosH     : SV_POSITION;
	float3 PosW     : POSITION0;
	float4 ShadowPosH : POSITION1;
	float2 Tex      : TEXCOORD0;
	float2 TiledTex : TEXCOORD1;
    float FogFactor : TEXCOORD2;
	float ClipValue : SV_ClipDistance0;//�ü�ֵ�ؼ���
};


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
	//float3 bitan = normalize(float3(0.0f, bottomY - topY, -2.0f * gWorldCellSpace));
	//float3 normalW = cross(tangent, bitan);
	
	// normal = (-dh/dx, 1, -dh/dz) : for height map normal
	float3 normalW = normalize(float3(leftY - rightY, 2.0f * gWorldCellSpace, topY - bottomY));
	
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
	float3 ModeSpacetoEye = WorldSpaceToTBN(toEye, normalW, tangent);

	uint  DiffuseMapSrvIndex = Mat.DiffuseMapIndex;
	uint  HeightMapSrvIndex = Mat.HeightMapIndex;
	uint  AOMapSrvIndex = Mat.AOMapIndex;
	uint  MetallicMapSrvIndex = Mat.MetallicMapIndex;
	uint  NormalMapSrvIndex = Mat.NormalMapIndex;
	uint  RoughnessMapSrvIndex = Mat.RoughnessMapIndex;
	float4 c[5];
	for (int i = 0; i < 3; ++i)
	{
		if (/*!isReflection*/1)
		{
			//�Ӳ�ӳ��
			float2 UV = ParallaxMapping(HeightMapSrvIndex, pin.TiledTex, ModeSpacetoEye, Mat.height_scale);

			float4 diffuseAlbedo = gSRVMap[DiffuseMapSrvIndex].Sample(gsamAnisotropicWrap, UV);
			float Roughness =1;// gSRVMap[RoughnessMapSrvIndex].Sample(gsamLinearWrap, UV).x;
			//float Metallic = gSRVMap[MetallicMapSrvIndex].Sample(gsamLinearWrap, UV).x;
			float AO = gSRVMap[AOMapSrvIndex].Sample(gsamLinearWrap, UV).x;
		
			// Indirect lighting.
			float4 ambient = gAmbientLight * diffuseAlbedo * AO*0.5f;

			//Sample normal
			float3 normalMapSample = gSRVMap[NormalMapSrvIndex].Sample(gsamLinearWrap, UV).xyz;
			//tansform normal
			float3 bumpedNormalW = NormalSampleToModelSpace(normalMapSample, normalW, tangent);
			bumpedNormalW = normalize(bumpedNormalW);

			Material mat = { diffuseAlbedo, Mat.FresnelR0, Roughness,0,AO };

			// Only the first light casts a shadow.
			float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
            if (gShadowEnabled)
            {
                shadowFactor[0] = CalcShadowFactor(pin.ShadowPosH);
            }  
			float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
				bumpedNormalW, toEye, shadowFactor);

            c[i] = ambient + directLight;

		}
		else
		{
			float4 diffuseAlbedo = gSRVMap[DiffuseMapSrvIndex].Sample(gsamLinearWrap, pin.TiledTex);
			c[i] = diffuseAlbedo;
		}
		DiffuseMapSrvIndex += 6;
		HeightMapSrvIndex += 6;
		AOMapSrvIndex += 6;
		MetallicMapSrvIndex += 6;
		NormalMapSrvIndex += 6;
		RoughnessMapSrvIndex += 6;
	}


	// Sample the blend map.
	float4 t = gSRVMap[mBlendMapIndex].Sample(gsamLinearWrap, pin.Tex);

	// Blend the layers on top of each other.
	float4 texColor = c[1];
	texColor = lerp(texColor, c[2], t.r);
	//texColor = lerp(texColor, c[3], t.g);
	//texColor = lerp(texColor, c[3], t.b);
	//texColor = lerp(texColor, c[4], t.a);


	//
	// Fogging
	//
	if (gFogEnabled)
	{
		// Blend the fog color and the lit color.
		texColor.rgb = lerp(texColor.rgb, gFogColor.rgb, pin.FogFactor);
	}
	//Gamma to nonlinear space
    float3 gamma = float(1.0 / 2.2);
    texColor.rgb = pow(texColor.rgb, gamma);


	return texColor;
}