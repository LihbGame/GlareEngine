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

    // Early-out for dominant layer
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

float4 main(ClipmapDomainOut pin) : SV_TARGET
{
    // DEBUG: output geometric normal as color (R=X, G=Y, B=Z mapped to [0,1])
    float3 n = normalize(pin.NormalW);
    return float4(n * 0.5 + 0.5, 1.0);
}
