#include "TerrainCommon.hlsli"
#include "TerrainNoise.hlsli"

RWTexture2D<float>      gOutputHeightMap      : register(u0);
RWTexture2D<float4>     gOutputNormalMap      : register(u1);
RWTexture2D<float4>     gOutputMaterialWeights : register(u2);

// Material weight computation.
// Layer indices match C++ texture order: 0=grass, 1=lightdirt, 2=darkdirt, 3=stone, 4=snow
float4 ComputeMaterialWeights(float height, float slope, float2 worldXZ)
{
    float macroNoise = TerrainGradientNoise(worldXZ * 0.008);
    float microNoise = TerrainGradientNoise(worldXZ * 0.05);

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
    float stone = max(slopeStone, heightStone) * (1.0 - snow);

    // Grass: moderate slopes, wide height band, noise-gated
    float grassBand = smoothstep(-snowH * 0.3, snowH * 0.1, height)
                    * (1.0 - smoothstep(snowH * 0.55, snowH * 0.8, height));
    float grassSlope = 1.0 - smoothstep(0.15, 0.5, slope);
    float grassNoise = smoothstep(0.3, 0.55, microNoise * 0.5 + 0.5);
    float grass = grassBand * grassSlope * grassNoise * (1.0 - snow) * (1.0 - stone);

    // Dark dirt: low-lying valleys
    float darkDirtHeight = 1.0 - smoothstep(-snowH * 0.5, -snowH * 0.1,
        height + macroNoise * snowH * 0.1);
    float darkDirt = darkDirtHeight * (1.0 - snow) * (1.0 - stone) * (1.0 - grass);

    // Light dirt: default base layer
    float lightDirt = max(0.0, 1.0 - snow - stone - grass - darkDirt);

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
    float3 result = ComputeTerrainHeightWithDerivatives(worldXZ,
        gNoiseScale, gNoiseSeed, gNoiseOctaves,
        gNoiseLacunarity, gNoisePersistence,
        gNoiseWarpScale, gNoiseHeightScale,
        gNoiseHighFreqLayers);

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
