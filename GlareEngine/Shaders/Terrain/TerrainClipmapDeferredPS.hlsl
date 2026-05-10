#include "TerrainCommon.hlsli"

struct PBRParams
{
    float3  Albedo;
    float3  Normal;
    float   Roughness;
    float   Metallic;
    float   AO;
};

PBRParams BlendMaterialLayers(float4 weights, float2 tiledUV, float2 worldXZ, float detailFade)
{
    PBRParams result = (PBRParams)0;
    float w[5];
    w[0] = weights.r; // grass (layer 0)
    w[1] = weights.g; // lightdirt (layer 1)
    w[2] = weights.b; // darkdirt (layer 2)
    w[3] = weights.a; // stone (layer 3)
    w[4] = max(0, 1.0 - w[0] - w[1] - w[2] - w[3]); // snow (layer 4)

    float2 detailUV = worldXZ / gDetailScale;

    [unroll]
    for (int i = 0; i < TERRAIN_NUM_LAYERS; i++)
    {
        if (w[i] < 0.01) continue;

        float3 albedo = SampleStochastic(gTerrainLayerAlbedo[i].x, tiledUV, worldXZ);
        float3 normalSample = SampleStochasticNormal(gTerrainLayerNormal[i].x, tiledUV, worldXZ);
        float roughness = SampleStochasticScalar(gTerrainLayerRoughness[i].x, tiledUV, worldXZ) * gTerrainRoughnessScale;
        float metallic = SampleStochasticScalar(gTerrainLayerMetallic[i].x, tiledUV, worldXZ) * gTerrainMetallicScale;
        float ao = SampleStochasticScalar(gTerrainLayerAO[i].x, tiledUV, worldXZ);

        // Detail overlay with distance fade
        if (detailFade > 0.0 && gDetailLayerAlbedo[i].x > 0)
        {
            float3 detailAlb = SampleStochastic(gDetailLayerAlbedo[i].x, detailUV, worldXZ);
            float3 detailNrm = SampleStochasticNormal(gDetailLayerNormal[i].x, detailUV, worldXZ);
            float  detailRgh = SampleStochasticScalar(gDetailLayerRoughness[i].x, detailUV, worldXZ);

            // Albedo: multiply with bias (0.5 = neutral, preserves base)
            albedo = lerp(albedo, albedo * (detailAlb * 2.0), detailFade);
            // Normal: direct blend (detail replaces base near camera)
            normalSample = lerp(normalSample, detailNrm, detailFade);
            // Roughness: multiply with bias (0.5 = neutral)
            roughness = lerp(roughness, roughness * (detailRgh * 2.0), detailFade);
        }

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

    // Compute TBN for normal map decoding
    float3 N = normalize(pin.NormalW);
    float3 T = normalize(pin.TangentW);
    T = normalize(T - dot(T, N) * N);
    float3 B = normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);

    float detailFade = saturate(1.0 - distance(pin.PosW, gTerrainEyePosW) / gDetailFadeDistance);
    PBRParams mat = BlendMaterialLayers(pin.MatWeights, tiledUV, pin.WorldXZ, detailFade);

    // Decode normal from blended normal map sample
    float3 normalT = 2.0 * mat.Normal - 1.0;
    float3 bumpedNormalW = normalize(mul(normalT, TBN));

    GBUFFER_BaseColor    = float4(mat.Albedo, mat.AO);
    GBUFFER_MSR          = float4(saturate(mat.Metallic), 1.0, saturate(max(mat.Roughness, 0.3)), 1);
    GBUFFER_Normal       = float4((bumpedNormalW + 1.0) / 2.0, 1.0);
    GBUFFER_Emissive     = float3(0, 0, 0);
    GBUFFER_WorldTangent = float4((T + 1.0) / 2.0, 1.0);
    float2 cancelJitter = gTerrainPreJitter - gTerrainCurJitter;
    GBUFFER_MotionVector = (((pin.PrePosition.xy / pin.PrePosition.z) - (pin.CurPosition.xy / pin.CurPosition.z)) - cancelJitter) * float2(0.5, -0.5);
}
