#include "../Misc/CommonResource.hlsli"

float4 main(PosNorTanTexOut pin) : SV_Target
{
// Vector from point being lit to eye. 
    float3 toEyeW = normalize(gEyePosW - pin.PosW);
// Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);
    pin.TangentW = normalize(pin.TangentW);

// Fetch the material data.
    MaterialData matData = gMaterialData[pin.MatIndex];

    float2 UV = pin.TexC;
//ÊÓ²îÕÚµ²Ó³Éä
    if (matData.mHeightScale>0.0f)
    {
        float3 ModeSpacetoEye = WorldSpaceToTBN(toEyeW, pin.NormalW, pin.TangentW);
        UV = ParallaxMapping(matData.mHeightMapIndex, pin.TexC, ModeSpacetoEye, matData.mHeightScale);
    }


    float4 diffuseAlbedo = gSRVMap[matData.mDiffuseMapIndex].Sample(gSamplerAnisoWrap, UV) ;
    float Roughness =gSRVMap[matData.mRoughnessMapIndex].Sample(gSamplerAnisoWrap, UV).x;
    float Metallic = gSRVMap[matData.mMetallicMapIndex].Sample(gSamplerAnisoWrap, UV).x;
    float AO = gSRVMap[matData.mAOMapIndex].Sample(gSamplerAnisoWrap, UV).x;
 //Sample normal
    float3 normalMapSample = gSRVMap[matData.mNormalMapIndex].Sample(gSamplerAnisoWrap, UV).xyz;
//tansform normal
    float3 bumpedNormalW = NormalSampleToModelSpace(normalMapSample, pin.NormalW, pin.TangentW);
    bumpedNormalW = normalize(bumpedNormalW);
    

// Indirect lighting.[IBL][GI]
    float NdotV = saturate(dot(bumpedNormalW, toEyeW));
    float3 F = fresnelSchlickRoughness(NdotV, matData.mFresnelR0, Roughness);
    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - Metallic;
    float4 Diffuse = diffuseAlbedo * gCubeMaps[gBakingDiffuseCubeIndex].Sample(gSamplerLinearWrap, bumpedNormalW);
    float MaxReflectionLod = 4;
    float3 PrefilteredColor = gCubeMaps[gBakingPreFilteredEnvIndex].SampleLevel(gSamplerLinearClamp, reflect(-toEyeW, bumpedNormalW), Roughness*MaxReflectionLod).xyz;
    float2 BRDF = gSRVMap[gBakingIntegrationBRDFIndex].Sample(gSamplerLinearClamp, float2(NdotV, Roughness)).xy;
    float3 Specular = PrefilteredColor*(F * BRDF.x + BRDF.y);
    float4 ambient = float4((kD * Diffuse.rgb + Specular) * AO, 1.0f);


    Material mat = { diffuseAlbedo, matData.mFresnelR0, Roughness, Metallic, AO };
// Only the first light casts a shadow.
    float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
    shadowFactor[0] = CalcShadowFactor(pin.ShadowPosH);
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
	bumpedNormalW, toEyeW, shadowFactor);

    float4 litColor = (ambient + directLight) * shadowFactor[0];

// Common convention to take alpha from diffuse material.
    litColor.a = diffuseAlbedo.a;

#ifdef ALPHA_TEST
clip(litColor.a - 0.1f);
#endif
    return litColor;
}