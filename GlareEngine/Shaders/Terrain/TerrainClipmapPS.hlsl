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
    w[0] = weights.r; // grass
    w[1] = weights.g; // lightdirt
    w[2] = weights.b; // darkdirt
    w[3] = weights.a; // stone
    w[4] = max(0, 1.0 - w[0] - w[1] - w[2] - w[3]); // snow

    // Early-out for dominant layer
    [unroll]
    for (int i = 0; i < TERRAIN_NUM_LAYERS; i++)
    {
        if (w[i] < 0.01) continue;

        float3 albedo = SampleStochastic(gTerrainLayerAlbedo[i].x, tiledUV, worldXZ);
        float3 normalSample = SampleStochasticNormal(gTerrainLayerNormal[i].x, tiledUV, worldXZ);
        float roughness = SampleStochasticScalar(gTerrainLayerRoughness[i].x, tiledUV, worldXZ) * gTerrainRoughnessScale;
        float metallic = SampleStochasticScalar(gTerrainLayerMetallic[i].x, tiledUV, worldXZ) * gTerrainMetallicScale;
        float ao = SampleStochasticScalar(gTerrainLayerAO[i].x, tiledUV, worldXZ);

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
    float2 tiledUV = pin.WorldXZ / gTerrainTexScale;

    PBRParams mat = BlendMaterialLayers(pin.MatWeights, tiledUV, pin.WorldXZ);

    // Decode normal from blended normal map sample
    float3 normalT = 2.0 * mat.Normal - 1.0;
    float3 N = normalize(pin.NormalW);
    float3 T = normalize(pin.TangentW);
    T = normalize(T - dot(T, N) * N);
    float3 B = normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);
    float3 bumpedNormalW = normalize(mul(normalT, TBN));

    // Simple directional light for forward path
    float3 lightDir = normalize(float3(0.5, 0.8, 0.3));
    float3 lit = mat.Albedo * max(dot(bumpedNormalW, lightDir), 0.3);

    return float4(lit, 1.0);
}
