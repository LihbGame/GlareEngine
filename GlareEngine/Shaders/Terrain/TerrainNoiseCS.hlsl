#include "TerrainCommon.hlsli"
#include "TerrainNoise.hlsli"

RWTexture2D<float>      gOutputHeightMap      : register(u0);
RWTexture2D<float4>     gOutputNormalMap      : register(u1);
RWTexture2D<float4>     gOutputMaterialWeights : register(u2);

float ComputeTerrainCurvature(float height, float2 worldXZ)
{
    float sampleStep = max(gNoiseCellSize * 8.0, 1.0);
    float hL = ComputeTerrainHeight(worldXZ + float2(-sampleStep, 0.0), gNoiseHeightScale);
    float hR = ComputeTerrainHeight(worldXZ + float2(sampleStep, 0.0), gNoiseHeightScale);
    float hD = ComputeTerrainHeight(worldXZ + float2(0.0, -sampleStep), gNoiseHeightScale);
    float hU = ComputeTerrainHeight(worldXZ + float2(0.0, sampleStep), gNoiseHeightScale);
    float neighborAverage = (hL + hR + hD + hU) * 0.25;

    return clamp((neighborAverage - height) / max(gNoiseHeightScale * 0.08, 1.0), -1.0, 1.0);
}

float TerrainMaskFBM(float2 worldXZ, float frequency)
{
    float2 p = worldXZ * frequency;
    float value = 0.0;
    float amplitude = 0.5;
    float amplitudeSum = 0.0;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        value += TerrainGradientNoise(p) * amplitude;
        amplitudeSum += amplitude;
        p = mul(float2x2(0.8, 0.6, -0.6, 0.8), p * 2.03) + float2(13.7, 7.1);
        amplitude *= 0.5;
    }

    return value / max(amplitudeSum, 0.0001);
}

float ComputeGrassSuitability(float height, float slope, float2 worldXZ)
{
    float snowH = max(gNoiseSnowHeight, 1.0);

    float lowerBand = smoothstep(-snowH * 0.35, snowH * 0.05, height);
    float upperBand = 1.0 - smoothstep(snowH * 0.52, snowH * 0.82, height);
    float alpineBand = lowerBand * upperBand;

    float slopeLimit = max(gGrassMaxSlope, 0.05);
    float slopeHabitat = 1.0 - smoothstep(slopeLimit * 0.55, slopeLimit, slope);

    float patchiness = saturate(gGrassPatchiness);
    float macroPatchNoise = TerrainGradientNoise(worldXZ * 0.004) * 0.5 + 0.5;
    float patchThreshold = lerp(0.20, 0.62, patchiness);
    float patchSoftness = lerp(0.35, 0.12, patchiness);
    float meadowPatch = smoothstep(patchThreshold - patchSoftness, patchThreshold + patchSoftness, macroPatchNoise);

    float curvature = ComputeTerrainCurvature(height, worldXZ);
    float moisture = lerp(0.65, 1.25, smoothstep(-0.35, 0.55, curvature));
    float moistureInfluence = lerp(1.0, moisture, saturate(gGrassMoistureBias));

    float fineNoise = TerrainGradientNoise(worldXZ * 0.035) * 0.5 + 0.5;
    float edgeBreakup = lerp(1.0, smoothstep(0.25, 0.75, fineNoise), patchiness * 0.35);

    return alpineBand * slopeHabitat * meadowPatch * moistureInfluence * edgeBreakup * max(gGrassCoverage, 0.0);
}

// Material weight computation.
// Layer indices match C++ texture order: 0=grass, 1=lightdirt, 2=darkdirt, 3=stone, 4=snow
float4 ComputeMaterialWeights(float height, float slope, float2 worldXZ)
{
    float snowH = gNoiseSnowHeight;
    float snowTransition = max(gNoiseSnowTransition, 1.0);
    float normalY = saturate(1.0 - slope);
    float macroNoise = TerrainMaskFBM(worldXZ, 0.0035);

    // Snow follows the Elevated-style score: elevation, broad FBM snowline drift, and upward-facing accumulation.
    float snowLineDrift = macroNoise * snowTransition * 2.4;
    float snowHeightScore = (height - snowH + snowLineDrift) / snowTransition;
    float snowFacingScore = (normalY - 0.58) * 1.85;
    float baseSnow = smoothstep(-1.15, 1.15, snowHeightScore + snowFacingScore);
    float snowAccumulation = smoothstep(0.08, 0.62, normalY);
    float snow = baseSnow * snowAccumulation;

    float snowEdgeMask = snow * (1.0 - snow) * 4.0;
    float snowEdgeNoise = TerrainMaskFBM(worldXZ, 0.026);
    float snowGranularNoise = TerrainGradientNoise(worldXZ * 0.095) * 0.5 + 0.5;
    float snowEdgeBreakup = snowEdgeNoise * 0.24 + (snowGranularNoise - 0.5) * 0.16;
    snow = saturate((snow + snowEdgeBreakup * snowEdgeMask * snowAccumulation) * max(gSnowCoverage, 0.0));
    float snowStoneExclusion = 1.0 - smoothstep(0.65, 0.98, snow);
    float snowDirtExclusion = 1.0 - smoothstep(0.02, 0.18, snow);

    // Stone favors steep faces and high exposed ridges, with crevice noise breaking the boundary.
    float slopeStone = smoothstep(
        gNoiseStoneSlope - gNoiseStoneTransition,
        gNoiseStoneSlope + gNoiseStoneTransition,
        slope);
    float exposureNoise = TerrainMaskFBM(worldXZ + float2(41.0, 19.0), 0.006);
    float heightStone = smoothstep(snowH * 0.58, snowH * 0.94,
        height + exposureNoise * snowH * 0.18);
    float rockRidgeNoise = TerrainMaskFBM(worldXZ + float2(11.0, 37.0), 0.018);
    float rockRidges = 1.0 - abs(rockRidgeNoise);
    float rockFaceBreakup = lerp(0.82, 1.18, smoothstep(0.28, 0.86, rockRidges));
    float stoneSource = saturate(max(slopeStone, heightStone * 0.72) * rockFaceBreakup);
    float stoneBase = stoneSource * snowStoneExclusion * 1.28;
    float creviceNoise = TerrainGradientNoise(worldXZ * 0.078);
    float creviceRidges = 1.0 - abs(creviceNoise);
    float screeNoise = TerrainGradientNoise(worldXZ * 0.18) * 0.5 + 0.5;
    float rockCreviceMask = smoothstep(0.58, 0.90, creviceRidges)
        * (0.50 + 0.50 * screeNoise)
        * saturate(stoneBase)
        * (0.30 + 0.70 * saturate(slopeStone))
        * snowStoneExclusion;
    float scree = rockCreviceMask * 0.50;
    float stone = saturate(stoneBase * (1.0 - scree * 0.55) + rockCreviceMask * 0.18);

    // Grass: alpine meadows favor snowline-adjacent gentle slopes, benches, and concave moisture pockets.
    float grass = ComputeGrassSuitability(height, slope, worldXZ) * (1.0 - snow) * (1.0 - saturate(slopeStone) * 0.85);

    // Dark dirt: low-lying valleys
    float darkDirtHeight = 1.0 - smoothstep(-snowH * 0.5, -snowH * 0.1,
        height + macroNoise * snowH * 0.1);
    float darkDirt = darkDirtHeight * (1.0 - snow) * (1.0 - saturate(stone)) * 0.9;

    // Light dirt: default base layer, softly suppressed near snow and zero in the snow core.
    float lightDirt = max(0.05 * snowDirtExclusion,
        snowDirtExclusion * (1.0 - saturate(stone)) * (1.0 - saturate(grass)) * 0.65);
    lightDirt += scree * stoneBase * snowDirtExclusion;

    // Pack into RGBA matching texture layer order: grass, lightdirt, darkdirt, stone
    float w[5];
    w[0] = grass;
    w[1] = lightDirt;
    w[2] = darkDirt;
    w[3] = stone;
    w[4] = snow;

    float sum = w[0] + w[1] + w[2] + w[3] + w[4] + 0.0001;
    return float4(w[0] / sum, w[1] / sum, w[2] / sum, w[3] / sum);
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= (uint)gNoiseHeightmapSize || DTid.y >= (uint)gNoiseHeightmapSize)
        return;

    // Map the full heightmap resolution across one terrain tile.
    float2 tileUV = (float2)DTid.xy / max((float)(gNoiseHeightmapSize - 1), 1.0);
    float2 worldXZ = ((float2)gNoiseTileOffset + tileUV * (float)gNoiseTileSize) * gNoiseCellSize;

    // Single evaluation: height + analytical gradient
    float3 result = ComputeTerrainHeightWithDerivatives(worldXZ, gNoiseHeightScale);

    float height = result.x;
    float2 gradient = result.yz;

    // Surface normal from analytical gradient: N = normalize(-dh/dx, 1, -dh/dz)
    float3 normal = normalize(float3(-gradient.x, 1.0, -gradient.y));
    float slope = 1.0 - normal.y; // 0=flat, 1=vertical

    // Compute material weights
    float4 weights = ComputeMaterialWeights(height, slope, worldXZ);

    // Write outputs
    gOutputHeightMap[DTid.xy] = height / (2.0 * gNoiseHeightScale) + 0.5;
    gOutputNormalMap[DTid.xy] = float4(normal, 1.0);
    gOutputMaterialWeights[DTid.xy] = weights;
}
