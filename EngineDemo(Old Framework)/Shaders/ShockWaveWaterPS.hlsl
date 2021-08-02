#include "Fog.hlsli"
#include "TerrainConstBuffer.hlsli"

#define FoamMax 10.0f
 
struct VertexOut
{
    float4 PosH       : SV_POSITION;
    float3 Eye        : TEXCOORD0;
    float4 Wave0      : TEXCOORD1;
    float2 Wave1      : TEXCOORD2;
    float2 Wave2      : TEXCOORD3;
    float2 Wave3      : TEXCOORD4;
    float4 ScreenPos  : TEXCOORD5;
    float2 HeighTex   : TEXCOORD6;
    float3 PosW       : TEXCOORD7;
    float time        : Time;
};


float3x3 GetTangentSpaceBasis(float3 T, float3 N)
{
    float3x3 objToTangentSpace;

    objToTangentSpace[0] = T;           // tangent
    objToTangentSpace[1] = cross(T,N); // binormal
    objToTangentSpace[2] = N;           // normal  

    return objToTangentSpace;
}

// This assumes NdotL comes clamped
half Fresnel(half NdotL, half fresnelBias, half fresnelPow)
{
    half facing = (1.0 - NdotL);
    return  max(fresnelBias + (1.0 - fresnelBias) * pow(facing, fresnelPow), 0.0);
}


float4 PS(VertexOut pin) : SV_Target
{
    float3 vEye = normalize(pin.Eye);

    // Get bump layers WavesBump
    float3 vBumpTexA = gSRVMap[gWaterDumpWaveIndex].Sample(gsamLinearWrap, pin.Wave0.xy).xyz;
    float3 vBumpTexB = gSRVMap[gWaterDumpWaveIndex].Sample(gsamLinearWrap, pin.Wave1.xy).xyz;
    float3 vBumpTexC = gSRVMap[gWaterDumpWaveIndex].Sample(gsamLinearWrap, pin.Wave2.xy).xyz;
    float3 vBumpTexD = gSRVMap[gWaterDumpWaveIndex].Sample(gsamLinearWrap, pin.Wave3.xy).xyz;

    // Average bump layers
    float3 vBumpTex = normalize(2.0 * (vBumpTexA.xyz + vBumpTexB.xyz + vBumpTexC.xyz + vBumpTexD.xyz) - 4.0);

    // Apply individual bump scale for refraction and reflection
    float3 vRefrBump = vBumpTex.xyz * float3(0.01, 0.01, 1.0);
    float3 vReflBump = vBumpTex.xyz * float3(0.01, 0.01, 1.0);


    // Compute projected coordinates
    float2 vProj = (pin.ScreenPos.xy / pin.ScreenPos.w);
    float4 vReflection = gSRVMap[gWaterReflectionMapIndex].Sample(gsamLinearClamp, vProj.xy + vReflBump.xy);
    float4 vRefrA = gSRVMap[gWaterRefractionMapIndex].Sample(gsamLinearClamp, vProj.xy + vRefrBump.xy);
    float4 vRefrB = gSRVMap[gWaterRefractionMapIndex].Sample(gsamLinearClamp, vProj.xy);

    // Mask occluders from refraction map
    float4 vRefraction = vRefrB * vRefrA.w + vRefrA * (1 - vRefrA.w);

    // Compute Fresnel term
    float NdotL = max(dot(vEye, vReflBump.xzy), 0);
    float facing = (1 - NdotL);
    float fresnel = Fresnel(NdotL, 0.01, 5.0);

    // Use distance to lerp between refraction and deep water color
    float fDistScale = saturate(gWaterTransparent / pin.Wave0.w);
    float3 WaterDeepColor = (vRefraction.xyz * fDistScale + (1 - fDistScale) * float3(0.0025, 0.1, 0.125));
    // Lerp between water color and deep water color
    float3 WaterColor = float3(0.05, 0.1, 0.15);
    float3 waterColor = (WaterColor* facing + WaterDeepColor * (1.0 - facing));
    float3 cReflect = fresnel * vReflection.xyz*0.5f ;


    float3 Foam = float3(0.0f, 0.0f, 0.0f);
    float time = pin.time;
    //Foam and heigh to water
    half Heigh = gSRVMap[mHeightMapIndex].SampleLevel(gsamLinearWrap, pin.HeighTex.xy, 0).r;
    if (-Heigh >= time && -Heigh <= time + 3.0f)
    {
        Foam.rgb = float3(1.0f, 1.0f, 1.0f);
        float timewave = time * (-Heigh / (time + 3.0f));
        Foam *= (1 - timewave) * 0.1f;
    }
    if (-Heigh < time || Heigh>time)
    {
      clip(-1.0f);
    }

    // final water = reflection_color * fresnel + water_color
    float4 texColor = float4(cReflect + waterColor + Foam, 1.0f);


    //lighting
    {
        Material mat = { float4(waterColor, 1.0f), float3(0.01f,0.01f,0.01f), 0.3f,0.0f,1.0f };
        // Only the first light casts a shadow.
        float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
        //shadowFactor[0] = CalcShadowFactor(pin.ShadowPosH);

        //tansform normal
        float3 bumpedNormalW = mul(vBumpTex * float3(0.6, 0.6, 1.0), GetTangentSpaceBasis(float3(1.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f)));
        bumpedNormalW = normalize(bumpedNormalW);

        texColor = ComputeLighting(gLights, mat, pin.PosW,
           bumpedNormalW, vEye, shadowFactor) + texColor;
    }

     // Fogging
     if (gFogEnabled)
     {
     // Blend the fog color and the lit color.
        texColor.rgb = lerp(texColor.rgb, gFogColor.rgb, ExponentialFog(0.5, length(pin.Eye)));
    }

     return texColor;
}