#include "TerrainCommon.hlsli"

struct PBRParams
{
    float3  Albedo;
    float3  Normal;
    float   Roughness;
    float   Metallic;
    float   AO;
};

PBRParams BlendMaterialLayers(float4 weights, float2 tiledUV, float2 worldXZ)
{
    PBRParams result = (PBRParams)0;
    float w[5];
    w[0] = weights.r; // lightdirt
    w[1] = weights.g; // darkdirt
    w[2] = weights.b; // grass
    w[3] = weights.a; // stone
    w[4] = max(0, 1.0 - w[0] - w[1] - w[2] - w[3]); // snow

    [unroll]
    for (int i = 0; i < TERRAIN_NUM_LAYERS; i++)
    {
        if (w[i] < 0.01) continue;

        float3 albedo = SampleStochastic(gTerrainLayerAlbedo[i], tiledUV, worldXZ);
        float3 normalSample = SampleStochasticNormal(gTerrainLayerNormal[i], tiledUV, worldXZ);
        float roughness = SampleStochasticScalar(gTerrainLayerRoughness[i], tiledUV, worldXZ);
        float metallic = SampleStochasticScalar(gTerrainLayerMetallic[i], tiledUV, worldXZ);
        float ao = SampleStochasticScalar(gTerrainLayerAO[i], tiledUV, worldXZ);

        result.Albedo += albedo * w[i];
        result.Normal += normalSample * w[i];
        result.Roughness += roughness * w[i];
        result.Metallic += metallic * w[i];
        result.AO += ao * w[i];
    }

    return result;
}

void main(ClipmapDomainOut pin,
    out float3 GBUFFER_Emissive     : SV_Target0,
    out float4 GBUFFER_Normal       : SV_Target1,
    out float4 GBUFFER_MSR          : SV_Target2,
    out float4 GBUFFER_BaseColor    : SV_Target3,
    out float4 GBUFFER_WorldTangent : SV_Target4,
    out float2 GBUFFER_MotionVector : SV_Target5)
{
    float2 tiledUV = pin.WorldXZ / gTerrainTexScale;

    PBRParams mat = BlendMaterialLayers(pin.MatWeights, tiledUV, pin.WorldXZ);

    // Decode normal from blended normal map sample
    float3 normalT = 2.0 * mat.Normal - 1.0;
    float3 N = normalize(pin.NormalW);
    float3 T = normalize(pin.TangentW);
    T = normalize(T - dot(T, N) * N);
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);
    float3 bumpedNormalW = normalize(mul(normalT, TBN));

    GBUFFER_BaseColor    = float4(mat.Albedo, mat.AO);
    GBUFFER_MSR          = float4(mat.Metallic, 1.0, mat.Roughness, 1);
    GBUFFER_Normal       = float4((bumpedNormalW + 1.0) / 2.0, 1.0);
    GBUFFER_Emissive     = float3(0, 0, 0);
    GBUFFER_WorldTangent = float4((T + 1.0) / 2.0, 1.0);
    GBUFFER_MotionVector = float2(0, 0);
}
