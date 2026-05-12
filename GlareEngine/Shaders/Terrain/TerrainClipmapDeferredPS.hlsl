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
    float w[5];
    w[0] = weights.r; // grass (layer 0)
    w[1] = weights.g; // lightdirt (layer 1)
    w[2] = weights.b; // darkdirt (layer 2)
    w[3] = weights.a; // stone (layer 3)
    w[4] = max(0, 1.0 - w[0] - w[1] - w[2] - w[3]); // snow (layer 4)

    float2 detailUV = worldXZ / gDetailScale;

    // Sample all active layers first (needed for height-based blending)
    float3 layerAlbedo[5];
    float3 layerNormal[5];
    float  layerRoughness[5];
    float  layerMetallic[5];
    float  layerAO[5];
    float  layerHeight[5];

    [unroll]
    for (int i = 0; i < TERRAIN_NUM_LAYERS; i++)
    {
        if (w[i] < 0.01)
        {
            layerAlbedo[i] = 0; layerNormal[i] = 0;
            layerRoughness[i] = 0; layerMetallic[i] = 0;
            layerAO[i] = 0; layerHeight[i] = 0;
            continue;
        }

        float3 albedo = SampleStochastic(gTerrainLayerAlbedo[i].x, tiledUV, worldXZ);
        float3 normalSample = SampleStochasticNormal(gTerrainLayerNormal[i].x, tiledUV, worldXZ);
        float roughness = SampleStochasticScalar(gTerrainLayerRoughness[i].x, tiledUV, worldXZ) * gTerrainRoughnessScale;
        float metallic = SampleStochasticScalar(gTerrainLayerMetallic[i].x, tiledUV, worldXZ) * gTerrainMetallicScale;
        float ao = SampleStochasticScalar(gTerrainLayerAO[i].x, tiledUV, worldXZ);

        // Detail overlay with distance fade (reuse base texture at higher tiling)
        if (detailFade > 0.0 && gDetailLayerAlbedo[i].x > 0)
        {
            float3 detailAlb = SampleStochastic(gDetailLayerAlbedo[i].x, detailUV, worldXZ);
            float3 detailNrm = SampleStochasticNormal(gDetailLayerNormal[i].x, detailUV, worldXZ);
            float  detailRgh = SampleStochasticScalar(gDetailLayerRoughness[i].x, detailUV, worldXZ);

            // Direct lerp: same texture at higher frequency, no darkening
            albedo = lerp(albedo, detailAlb, detailFade);
            normalSample = lerp(normalSample, detailNrm, detailFade);
            roughness = lerp(roughness, detailRgh * gTerrainRoughnessScale, detailFade);
        }

        layerAlbedo[i] = albedo;
        layerNormal[i] = normalSample;
        layerRoughness[i] = roughness;
        layerMetallic[i] = metallic;
        layerAO[i] = ao;
        // Use height map for transition sharpness (fallback to luminance if disabled)
        layerHeight[i] = (gTerrainUseHeightBlend)
            ? SampleStochasticScalar(gTerrainLayerHeight[i].x, tiledUV, worldXZ)
            : dot(albedo, float3(0.299, 0.587, 0.114));
    }

    // Height-based weight adjustment: brighter texels punch through in transition zones
    float blendSharpness = 2.0;
    float adjustedW[5];
    float totalW = 0.0;

    [unroll]
    for (int j = 0; j < TERRAIN_NUM_LAYERS; j++)
    {
        if (w[j] < 0.01) { adjustedW[j] = 0; continue; }
        adjustedW[j] = w[j] * (layerHeight[j] * blendSharpness + 0.01);
        totalW += adjustedW[j];
    }

    PBRParams result = (PBRParams)0;
    if (totalW > 0.0)
    {
        [unroll]
        for (int k = 0; k < TERRAIN_NUM_LAYERS; k++)
        {
            float fw = adjustedW[k] / totalW;
            result.Albedo += layerAlbedo[k] * fw;
            result.Normal += layerNormal[k] * fw;
            result.Roughness += layerRoughness[k] * fw;
            result.Metallic += layerMetallic[k] * fw;
            result.AO += layerAO[k] * fw;
        }
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

    float detailFade = saturate(1.0 - distance(pin.PosW, gTerrainEyePosW) / gDetailFadeDistance) * 0.8;
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
