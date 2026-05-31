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
    float macroNoise = TerrainGradientNoise(worldXZ * 0.008);

    float snowH = gNoiseSnowHeight;

    // Snow: high elevation
    float snow = smoothstep(snowH - gNoiseSnowTransition, snowH + gNoiseSnowTransition, height);

    // Stone: steep slopes + high elevation
    float slopeStone = smoothstep(
        gNoiseStoneSlope - gNoiseStoneTransition,
        gNoiseStoneSlope + gNoiseStoneTransition,
        slope);
    float heightStone = smoothstep(snowH * 0.7, snowH * 0.95,
        height + macroNoise * snowH * 0.2);
    float stone = max(slopeStone, heightStone) * (1.0 - snow) * 1.35;

    // Grass: alpine meadows favor snowline-adjacent gentle slopes, benches, and concave moisture pockets.
    float grass = ComputeGrassSuitability(height, slope, worldXZ) * (1.0 - snow) * (1.0 - saturate(slopeStone) * 0.85);

    // Dark dirt: low-lying valleys
    float darkDirtHeight = 1.0 - smoothstep(-snowH * 0.5, -snowH * 0.1,
        height + macroNoise * snowH * 0.1);
    float darkDirt = darkDirtHeight * (1.0 - snow) * (1.0 - saturate(stone)) * 0.9;

    // Light dirt: default base layer
    float lightDirt = max(0.05, (1.0 - snow) * (1.0 - saturate(stone)) * (1.0 - saturate(grass)) * 0.65);

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
