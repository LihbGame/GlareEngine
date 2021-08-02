#include "Common.hlsli"


struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float4 ShadowPosH : POSITION0;
	float3 PosW    : POSITION1;
	float3 NormalW : NORMAL;
	float3 TangentW:TANGENT;
	float2 TexC    : TEXCOORD;
	nointerpolation uint MatIndex  : MATINDEX;
};


float4 PS(VertexOut pin) : SV_Target
{
	// Vector from point being lit to eye. 
	float3 toEyeW = normalize(gEyePosW - pin.PosW);
	// Interpolating normal can unnormalize it, so renormalize it.
	pin.NormalW = normalize(pin.NormalW);
	pin.TangentW = normalize(pin.TangentW);

	// Fetch the material data.
	MaterialData matData = gMaterialData[pin.MatIndex];

	float4 diffuseAlbedo = gSRVMap[matData.DiffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
	float Roughness = gSRVMap[matData.RoughnessMapIndex].Sample(gsamAnisotropicWrap, pin.TexC).x;
	float Metallic = gSRVMap[matData.MetallicMapIndex].Sample(gsamAnisotropicWrap, pin.TexC).x;
	float AO = gSRVMap[matData.AOMapIndex].Sample(gsamAnisotropicWrap, pin.TexC).x;


	// Indirect lighting.
	float4 ambient = gAmbientLight * diffuseAlbedo * AO;

	//Sample normal
	float3 normalMapSample = gSRVMap[matData.NormalMapIndex].Sample(gsamAnisotropicWrap, pin.TexC).xyz;
	//tansform normal
	float3 bumpedNormalW = NormalSampleToModelSpace(normalMapSample, pin.NormalW, pin.TangentW);
	bumpedNormalW = normalize(bumpedNormalW);

	Material mat = { diffuseAlbedo, matData.FresnelR0, Roughness,Metallic,AO };
	// Only the first light casts a shadow.
	float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
	shadowFactor[0] = CalcShadowFactor(pin.ShadowPosH);
	float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
		bumpedNormalW, toEyeW, shadowFactor);

	float4 litColor = ambient * shadowFactor[0] + directLight;

	// Common convention to take alpha from diffuse material.
	litColor.a = diffuseAlbedo.a;

#ifdef ALPHA_TEST
	clip(litColor.a - 0.1f);
#endif
		//Gamma to nonlinear space
    float3 gamma = float(1.0 / 2.2);
    litColor.rgb = pow(litColor.rgb, gamma);
	return litColor;
}