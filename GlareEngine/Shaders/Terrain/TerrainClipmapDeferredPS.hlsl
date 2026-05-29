#include "TerrainCommon.hlsli"

struct PBRParams
{
    float3  Albedo;
    float3  NormalW;
    float   Roughness;
    float   Metallic;
    float   AO;
};

PBRParams BlendMaterialLayers(
    float4 weights,
    float2 tiledUV,
    float2 tiledUvDx,
    float2 tiledUvDy,
    float2 detailUV,
    float detailFade,
    float2 detailUvDx,
    float2 detailUvDy,
    float3 posW,
    float3 posDx,
    float3 posDy,
    float3 surfaceNormalW,
    float3x3 tbn,
    float3 viewDirW,
    float3 viewDirT,
    float parallaxMaxLayers)
{
    float w[5];
    w[0] = weights.r; // grass (layer 0)
    w[1] = weights.g; // lightdirt (layer 1)
    w[2] = weights.b; // darkdirt (layer 2)
    w[3] = weights.a; // stone (layer 3)
    w[4] = max(0, 1.0 - w[0] - w[1] - w[2] - w[3]); // snow (layer 4)

    // Sample all active layers first (needed for height-based blending)
    float3 layerAlbedo[5];
    float3 layerNormalW[5];
    float  layerRoughness[5];
    float  layerMetallic[5];
    float  layerAO[5];
    float  layerHeight[5];
    float  triplanarFade = (gTerrainUseTriplanar != 0) ? TerrainTriplanarFade(surfaceNormalW) : 0.0;
    bool   useTriplanarProjection = triplanarFade > 0.001;
    bool   usePlanarProjection = triplanarFade < 0.999;

    [unroll]
    for (int i = 0; i < TERRAIN_NUM_LAYERS; i++)
    {
        if (w[i] < 0.01)
        {
            layerAlbedo[i] = 0; layerNormalW[i] = 0;
            layerRoughness[i] = 0; layerMetallic[i] = 0;
            layerAO[i] = 0; layerHeight[i] = 0;
            continue;
        }

        float layerParallaxFade = detailFade;
        bool hasParallax = (gTerrainUseParallax != 0 && gTerrainLayerHeight[i].x >= 0 && layerParallaxFade > 0.001);
        float planarParallaxFade = 1.0 - triplanarFade;
        bool hasPlanarParallax = hasParallax && planarParallaxFade > 0.001;
        float2 layerUV = tiledUV;
        float2 layerDetailUV = detailUV;
        if (hasPlanarParallax)
        {
            layerDetailUV = ParallaxMapping((uint)gTerrainLayerHeight[i].x, detailUV, viewDirT, viewDirW, surfaceNormalW,
                gTerrainParallaxHeightScale * layerParallaxFade * planarParallaxFade, parallaxMaxLayers);
        }

        bool useAlignedSampling = hasPlanarParallax;
        TerrainTriplanarUVSet baseTriplanarUV = (TerrainTriplanarUVSet)0;

        float3 planarAlbedo = 0.0;
        float3 planarNormalW = surfaceNormalW;
        float planarRoughness = 0.0;
        float planarMetallic = 0.0;
        float planarAO = 0.0;

        if (usePlanarProjection)
        {
            planarAlbedo = TerrainSamplePlanarRGB(gTerrainLayerAlbedo[i].x, layerUV, tiledUvDx, tiledUvDy, true);
            planarNormalW = TerrainPlanarNormalToWorld(TerrainSamplePlanarRGB(gTerrainLayerNormal[i].x, layerUV, tiledUvDx, tiledUvDy, true), tbn);
            planarRoughness = TerrainSamplePlanarScalar(gTerrainLayerRoughness[i].x, layerUV, tiledUvDx, tiledUvDy, true) * gTerrainRoughnessScale;
            planarMetallic = TerrainSamplePlanarScalar(gTerrainLayerMetallic[i].x, layerUV, tiledUvDx, tiledUvDy, true) * gTerrainMetallicScale;
            planarAO = TerrainSamplePlanarScalar(gTerrainLayerAO[i].x, layerUV, tiledUvDx, tiledUvDy, true);
        }

        float3 triAlbedo = 0.0;
        float3 triNormalW = surfaceNormalW;
        float triRoughness = 0.0;
        float triMetallic = 0.0;
        float triAO = 0.0;

        if (useTriplanarProjection)
        {
            baseTriplanarUV = TerrainBuildTriplanarUVSet(posW, posDx, posDy, surfaceNormalW, gTerrainTexScale);

            triAlbedo = TerrainSampleTriplanarRGB(gTerrainLayerAlbedo[i].x, baseTriplanarUV);
            triNormalW = TerrainSampleTriplanarNormalW(gTerrainLayerNormal[i].x, baseTriplanarUV);
            triRoughness = TerrainSampleTriplanarScalar(gTerrainLayerRoughness[i].x, baseTriplanarUV) * gTerrainRoughnessScale;
            triMetallic = TerrainSampleTriplanarScalar(gTerrainLayerMetallic[i].x, baseTriplanarUV) * gTerrainMetallicScale;
            triAO = TerrainSampleTriplanarScalar(gTerrainLayerAO[i].x, baseTriplanarUV);
        }

        float3 albedo;
        float3 normalW;
        float roughness;
        float metallic;
        float ao;

        if (useTriplanarProjection && usePlanarProjection)
        {
            albedo = lerp(planarAlbedo, triAlbedo, triplanarFade);
            normalW = normalize(lerp(planarNormalW, triNormalW, triplanarFade));
            roughness = lerp(planarRoughness, triRoughness, triplanarFade);
            metallic = lerp(planarMetallic, triMetallic, triplanarFade);
            ao = lerp(planarAO, triAO, triplanarFade);
        }
        else if (useTriplanarProjection)
        {
            albedo = triAlbedo;
            normalW = triNormalW;
            roughness = triRoughness;
            metallic = triMetallic;
            ao = triAO;
        }
        else
        {
            albedo = planarAlbedo;
            normalW = planarNormalW;
            roughness = planarRoughness;
            metallic = planarMetallic;
            ao = planarAO;
        }

        // Detail overlay with distance fade (reuse base texture at higher tiling)
        if (detailFade > 0.0 && gDetailLayerAlbedo[i].x > 0)
        {
            float3 planarDetailAlb = 0.0;
            float3 planarDetailNrmW = surfaceNormalW;
            float planarDetailRgh = 0.0;

            if (usePlanarProjection)
            {
                planarDetailAlb = TerrainSamplePlanarRGB(gDetailLayerAlbedo[i].x, layerDetailUV, detailUvDx, detailUvDy, useAlignedSampling);
                planarDetailNrmW = TerrainPlanarNormalToWorld(TerrainSamplePlanarRGB(gDetailLayerNormal[i].x, layerDetailUV, detailUvDx, detailUvDy, useAlignedSampling), tbn);
                planarDetailRgh = TerrainSamplePlanarScalar(gDetailLayerRoughness[i].x, layerDetailUV, detailUvDx, detailUvDy, useAlignedSampling);
            }

            float3 triDetailAlb = 0.0;
            float3 triDetailNrmW = surfaceNormalW;
            float triDetailRgh = 0.0;

            if (useTriplanarProjection)
            {
                TerrainTriplanarUVSet detailTriplanarUV = TerrainBuildTriplanarUVSet(posW, posDx, posDy, surfaceNormalW, gDetailScale);

                triDetailAlb = TerrainSampleTriplanarRGB(gDetailLayerAlbedo[i].x, detailTriplanarUV);
                triDetailNrmW = TerrainSampleTriplanarNormalW(gDetailLayerNormal[i].x, detailTriplanarUV);
                triDetailRgh = TerrainSampleTriplanarScalar(gDetailLayerRoughness[i].x, detailTriplanarUV);
            }

            float3 detailAlb;
            float3 detailNrmW;
            float detailRgh;
            if (useTriplanarProjection && usePlanarProjection)
            {
                detailAlb = lerp(planarDetailAlb, triDetailAlb, triplanarFade);
                detailNrmW = normalize(lerp(planarDetailNrmW, triDetailNrmW, triplanarFade));
                detailRgh = lerp(planarDetailRgh, triDetailRgh, triplanarFade);
            }
            else if (useTriplanarProjection)
            {
                detailAlb = triDetailAlb;
                detailNrmW = triDetailNrmW;
                detailRgh = triDetailRgh;
            }
            else
            {
                detailAlb = planarDetailAlb;
                detailNrmW = planarDetailNrmW;
                detailRgh = planarDetailRgh;
            }

            // Direct lerp: same texture at higher frequency, no darkening
            albedo = lerp(albedo, detailAlb, detailFade);
            normalW = normalize(lerp(normalW, detailNrmW, detailFade));
            roughness = lerp(roughness, detailRgh * gTerrainRoughnessScale, detailFade);
        }

        layerAlbedo[i] = albedo;
        layerNormalW[i] = normalW;
        layerRoughness[i] = roughness;
        layerMetallic[i] = metallic;
        layerAO[i] = ao;

        // Use height map for transition sharpness (fallback to luminance if disabled)
        if (gTerrainUseHeightBlend && gTerrainLayerHeight[i].x >= 0)
        {
            float planarHeight = usePlanarProjection
                ? TerrainSamplePlanarScalar(gTerrainLayerHeight[i].x, layerUV, tiledUvDx, tiledUvDy, true)
                : 0.0;
            float triHeight = useTriplanarProjection
                ? TerrainSampleTriplanarScalar(gTerrainLayerHeight[i].x, baseTriplanarUV)
                : 0.0;

            layerHeight[i] = (useTriplanarProjection && usePlanarProjection)
                ? lerp(planarHeight, triHeight, triplanarFade)
                : (useTriplanarProjection ? triHeight : planarHeight);
        }
        else
        {
            layerHeight[i] = dot(albedo, float3(0.299, 0.587, 0.114));
        }
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
            result.NormalW += layerNormalW[k] * fw;
            result.Roughness += layerRoughness[k] * fw;
            result.Metallic += layerMetallic[k] * fw;
            result.AO += layerAO[k] * fw;
        }
        result.NormalW = normalize(result.NormalW);
    }
    else
    {
        result.NormalW = surfaceNormalW;
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
    float2 tiledUvDx = ddx(tiledUV);
    float2 tiledUvDy = ddy(tiledUV);
    float2 detailUV = pin.WorldXZ / gDetailScale;
    float2 detailUvDx = ddx(detailUV);
    float2 detailUvDy = ddy(detailUV);
    float3 posDx = ddx(pin.PosW);
    float3 posDy = ddy(pin.PosW);

    // Compute TBN for normal map decoding
    float3 N = normalize(pin.NormalW);
    float3 T = normalize(pin.TangentW);
    T = normalize(T - dot(T, N) * N);
    float3 B = normalize(cross(T, N));
    float3x3 TBN = float3x3(T, B, N);
    float3 viewDirW = normalize(gTerrainEyePosW - pin.PosW);
    float3 viewDirT = mul(viewDirW, transpose(TBN));
    float distToEye = distance(pin.PosW, gTerrainEyePosW);
    float detailFade = saturate(1.0 - distToEye / gDetailFadeDistance) * 0.8;
    float parallaxStepFade = detailFade * detailFade * (3.0 - 2.0 * detailFade);
    float parallaxMaxLayers = lerp(0.0, 30.0, parallaxStepFade);
    PBRParams mat = BlendMaterialLayers(
        pin.MatWeights,
        tiledUV,
        tiledUvDx,
        tiledUvDy,
        detailUV,
        detailFade,
        detailUvDx,
        detailUvDy,
        pin.PosW,
        posDx,
        posDy,
        N,
        TBN,
        viewDirW,
        viewDirT,
        parallaxMaxLayers);

    float3 bumpedNormalW = normalize(mat.NormalW);

    GBUFFER_BaseColor    = float4(mat.Albedo, mat.AO);
    GBUFFER_MSR          = float4(saturate(mat.Metallic), 1.0, saturate(max(mat.Roughness, 0.3)), 1);
    GBUFFER_Normal       = float4((bumpedNormalW + 1.0) / 2.0, 1.0);
    GBUFFER_Emissive     = float3(0, 0, 0);
    GBUFFER_WorldTangent = float4((T + 1.0) / 2.0, 1.0);
    float2 cancelJitter = gTerrainPreJitter - gTerrainCurJitter;
    GBUFFER_MotionVector = (((pin.PrePosition.xy / pin.PrePosition.z) - (pin.CurPosition.xy / pin.CurPosition.z)) - cancelJitter) * float2(0.5, -0.5);
}
