#include "TerrainCommon.hlsli"
#include "TerrainNoise.hlsli"

RWTexture2D<float>      gOutputHeightMap      : register(u0);
RWTexture2D<float2>     gOutputNormalMap      : register(u1);
RWTexture2D<float4>     gOutputMaterialWeights : register(u2);

float4 ComputeMaterialWeights(float height, float slope, float2 worldXZ)
{
    float transitionNoise = GradientNoise(worldXZ * 0.05);

    // Layer indices: 0=lightdirt, 1=darkdirt, 2=grass, 3=stone, 4=snow
    float snow = smoothstep(
        gNoiseSnowHeight - gNoiseSnowTransition,
        gNoiseSnowHeight + gNoiseSnowTransition,
        height);

    float stone = smoothstep(
        gNoiseStoneSlope - gNoiseStoneTransition,
        gNoiseStoneSlope + gNoiseStoneTransition,
        slope);

    float grassMask = smoothstep(0.2, 0.6, transitionNoise + 0.5 - slope * 2.0);
    float darkDirtMask = smoothstep(-0.1, 0.3, transitionNoise - 0.2) * (1.0 - grassMask);

    // Priority-based weight computation
    float w[5];
    w[4] = snow;
    w[3] = (1.0 - snow) * stone;
    w[2] = (1.0 - snow) * (1.0 - stone) * grassMask;
    w[1] = (1.0 - snow) * (1.0 - stone) * (1.0 - grassMask) * darkDirtMask;
    w[0] = max(0.0, 1.0 - w[1] - w[2] - w[3] - w[4]);

    float sum = w[0] + w[1] + w[2] + w[3] + w[4] + 0.0001;
    return float4(w[0] / sum, w[1] / sum, w[2] / sum, w[3] / sum);
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= (uint)gNoiseHeightmapSize || DTid.y >= (uint)gNoiseHeightmapSize)
        return;

    // Compute world position for this texel
    float2 worldXZ = (gNoiseTileOffset + (int2)DTid.xy) * gNoiseCellSize;

    // Generate height using shared noise function
    float height = ComputeTerrainHeight(worldXZ,
        gNoiseScale, gNoiseSeed, gNoiseOctaves,
        gNoiseLacunarity, gNoisePersistence,
        gNoiseWarpStrength, gNoiseWarpScale, gNoiseHeightScale);

    // Compute normal via finite differences
    float eps = gNoiseCellSize;
    float hL = ComputeTerrainHeight(worldXZ + float2(-eps, 0),
        gNoiseScale, gNoiseSeed, gNoiseOctaves,
        gNoiseLacunarity, gNoisePersistence,
        gNoiseWarpStrength, gNoiseWarpScale, gNoiseHeightScale);
    float hR = ComputeTerrainHeight(worldXZ + float2( eps, 0),
        gNoiseScale, gNoiseSeed, gNoiseOctaves,
        gNoiseLacunarity, gNoisePersistence,
        gNoiseWarpStrength, gNoiseWarpScale, gNoiseHeightScale);
    float hD = ComputeTerrainHeight(worldXZ + float2(0, -eps),
        gNoiseScale, gNoiseSeed, gNoiseOctaves,
        gNoiseLacunarity, gNoisePersistence,
        gNoiseWarpStrength, gNoiseWarpScale, gNoiseHeightScale);
    float hU = ComputeTerrainHeight(worldXZ + float2(0,  eps),
        gNoiseScale, gNoiseSeed, gNoiseOctaves,
        gNoiseLacunarity, gNoisePersistence,
        gNoiseWarpStrength, gNoiseWarpScale, gNoiseHeightScale);

    float3 normal = normalize(float3(hL - hR, 2.0 * eps, hD - hU));
    float slope = 1.0 - normal.y; // 0=flat, 1=vertical

    // Compute material weights
    float4 weights = ComputeMaterialWeights(height, slope, worldXZ);

    // Pack normal (hemisphere encoding: normal is always pointing up)
    float2 packedNormal = normal.xz;

    // Write outputs
    // Store height normalized to [0, 1] for R32_FLOAT
    gOutputHeightMap[DTid.xy] = height / (2.0 * gNoiseHeightScale) + 0.5;
    gOutputNormalMap[DTid.xy] = packedNormal;
    gOutputMaterialWeights[DTid.xy] = weights;
}
