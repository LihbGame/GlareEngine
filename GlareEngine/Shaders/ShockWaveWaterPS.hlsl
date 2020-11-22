#include "Common.hlsli"

struct VertexOut
{
    float4 PosH    : SV_POSITION;
    float3 Eye        : TEXCOORD0;
    float4 Wave0      : TEXCOORD1;
    float2 Wave1      : TEXCOORD2;
    float2 Wave2      : TEXCOORD3;
    float2 Wave3      : TEXCOORD4;
    float4 ScreenPos  : TEXCOORD5;
    float2 HeighTex  : TEXCOORD6;
};


// This assumes NdotL comes clamped
half Fresnel(half NdotL, half fresnelBias, half fresnelPow)
{
    half facing = (1.0 - NdotL);
    return  max(fresnelBias + (1.0 - fresnelBias) * pow(facing, fresnelPow), 0.0);
}


float4 PS(VertexOut pin) : SV_Target
{
    float3 vEye = normalize(pin.Eye);

    // Get bump layers gSRVMap[51] WavesBump
    float3 vBumpTexA = gSRVMap[51].Sample(gsamLinearWrap, pin.Wave0.xy).xyz;
    float3 vBumpTexB = gSRVMap[51].Sample(gsamLinearWrap, pin.Wave1.xy).xyz;
    float3 vBumpTexC = gSRVMap[51].Sample(gsamLinearWrap, pin.Wave2.xy).xyz;
    float3 vBumpTexD = gSRVMap[51].Sample(gsamLinearWrap, pin.Wave3.xy).xyz;

    // Average bump layers
    float3 vBumpTex = normalize(2.0 * (vBumpTexA.xyz + vBumpTexB.xyz + vBumpTexC.xyz + vBumpTexD.xyz) - 4.0);

    // Apply individual bump scale for refraction and reflection
    float3 vRefrBump = vBumpTex.xyz * float3(0.02, 0.02, 1.0);
    float3 vReflBump = vBumpTex.xyz * float3(0.01, 0.01, 1.0);


    // Compute projected coordinates gSRVMap[50]:·´ÉäÎÆÀí  gSRVMap[49]£ºÕÛÉäÎÆÀí
    float2 vProj = (pin.ScreenPos.xy / pin.ScreenPos.w);
    float4 vReflection = gSRVMap[50].Sample(gsamLinearWrap, vProj.xy + vReflBump.xy);
    float4 vRefrA = gSRVMap[49].Sample(gsamLinearWrap, vProj.xy + vRefrBump.xy);
    float4 vRefrB = gSRVMap[49].Sample(gsamLinearWrap, vProj.xy);

    // Mask occluders from refraction map
    float4 vRefraction = vRefrB * vRefrA.w + vRefrA * (1 - vRefrA.w);

    // Compute Fresnel term
    float NdotL = max(dot(vEye, vReflBump), 0);
    float facing = (1.0 - NdotL);
    float fresnel = Fresnel(NdotL, 0.02, 5.0);

    // Use distance to lerp between refraction and deep water color
    float fDistScale = saturate(10.0 / pin.Wave0.w);
    float3 WaterDeepColor = (vRefraction.xyz * fDistScale + (1 - fDistScale) * float3(0, 0.1, 0.125));
    // Lerp between water color and deep water color
    float3 WaterColor = float3(0, 0.1, 0.15);
    float3 waterColor = (WaterColor * facing + WaterDeepColor * (1.0 - facing));
    float3 cReflect = fresnel * vReflection.xyz;

    //Foam and heigh to water
    //float Heigh = gHeighMap.Sample(gsamLinearWrap, pin.HeighTex.xy).r;

    //float4 Foam = float4(0.0f, 0.0f, 0.0f, 0.0f);
    // float time = abs(sin(gTotalTime));
    //if (-Heigh >= time && -Heigh <= time + gFoamMax)
    //{
       // Foam.rgb = gFoamMap.Sample(samLinear, pin.HeighTex.xy).rgb;
        //float timewave = time * (-Heigh / (time + gFoamMax));
        //Foam *= (1 - timewave) * 0.2f;
    //}
    /*if (-Heigh < time)
    {
        clip(-1.0f);
    }*/


    // final water = reflection_color * fresnel + water_color
    return float4(cReflect + waterColor/* + Foam*/, 1.0f);
}