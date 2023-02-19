#include "../Misc/PBRLighting.hlsli"

#define MAX_LUNINANCE 100

cbuffer CubePass : register(b1)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float ObjectPad1 = 0.0f;
    float2 RenderTargetSize = { 0.0f, 0.0f };
    float2 InvRenderTargetSize = { 0.0f, 0.0f };
    int mSkyCubeIndex = 0;
    float mRoughness = 0.0f;
}

float4 main(PosVSOut pin) : SV_TARGET
{
    float3 N = normalize(pin.PosL);
    float3 R = N;
    float3 V = R;

    const uint SAMPLE_COUNT = 1024u;
    float TotalWeight = 0.0f;
    float3 PrefilteredColor = float3(0.0f,0.0f,0.0f);
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, mRoughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0f)
        {
            float3 sampleColor = gCubeMaps[mSkyCubeIndex].Sample(gSamplerLinearWrap, L).rgb;
            sampleColor = clamp(sampleColor, 0.0f, MAX_LUNINANCE);
            PrefilteredColor += sampleColor * NdotL;
            TotalWeight += NdotL;
        }
    }
    PrefilteredColor /= TotalWeight;
    float4 FragColor = float4(PrefilteredColor, 1.0);
    return FragColor;
}