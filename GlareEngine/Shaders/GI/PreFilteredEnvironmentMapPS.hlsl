#include "../Misc/CommonResource.hlsli"

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // from spherical coordinates to cartesian coordinates
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // from tangent-space vector to world-space sample vector
    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

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
            PrefilteredColor += gCubeMaps[mSkyCubeIndex].Sample(gSamplerLinearWrap, L).rgb * NdotL;
            TotalWeight += NdotL;
        }
    }
    PrefilteredColor /= TotalWeight;
    float4 FragColor = float4(PrefilteredColor, 1.0);
    return FragColor;
}