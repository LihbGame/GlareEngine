#include "TerrainCommon.hlsli"

RWTexture2D<float>      gOutputHeightMap      : register(u0);
RWTexture2D<float2>     gOutputNormalMap      : register(u1);
RWTexture2D<float4>     gOutputMaterialWeights : register(u2);

// Seeded hash for reproducible terrain
float Hash2D(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float2 Hash2D2(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.xx + p3.yz) * p3.zy);
}

// Domain-warped FBM for natural terrain
float TerrainFBM(float2 pos)
{
    float value = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    float maxValue = 0.0;

    // Add seed-based offset
    float seedOffset = Hash2D(float2(gNoiseSeed, gNoiseSeed * 1.37));
    pos += seedOffset * 1000.0;

    float2x2 rot = { 0.8, 0.6, -0.6, 0.8 }; // rotation to reduce axis bias

    [unroll]
    for (uint i = 0; i < 8; i++)
    {
        if (i >= gNoiseOctaves) break;
        value += amplitude * GradientNoise(pos * frequency);
        maxValue += amplitude;
        frequency *= gNoiseLacunarity;
        amplitude *= gNoisePersistence;
        pos = mul(rot, pos) + float2(1.7, 9.2);
    }

    return value / maxValue;
}

// Ridge noise for mountain features
float TerrainRidgeNoise(float2 pos)
{
    float value = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    float maxValue = 0.0;

    float seedOffset = Hash2D(float2(gNoiseSeed * 2.17, gNoiseSeed * 0.73));
    pos += seedOffset * 800.0;

    float2x2 rot = { 0.8, 0.6, -0.6, 0.8 };

    [unroll]
    for (uint i = 0; i < 8; i++)
    {
        if (i >= gNoiseOctaves) break;
        float n = GradientNoise(pos * frequency);
        n = 1.0 - abs(n); // ridge transform
        n = n * n; // sharpen ridges
        value += amplitude * n;
        maxValue += amplitude;
        frequency *= gNoiseLacunarity;
        amplitude *= gNoisePersistence;
        pos = mul(rot, pos) + float2(5.3, 1.7);
    }

    return value / maxValue;
}

float ComputeHeight(float2 worldXZ)
{
    // Low-frequency continental base (large-scale mountains/valleys)
    float continental = TerrainFBM(worldXZ * gNoiseScale * 0.1);
    float baseMask = smoothstep(0.2, 0.8, continental);

    // Domain warping for organic shapes
    float2 warp = float2(
        TerrainFBM(worldXZ * gNoiseWarpScale + float2(0.0, 0.0)),
        TerrainFBM(worldXZ * gNoiseWarpScale + float2(5.2, 1.3))
    );
    float2 warpedXZ = worldXZ + warp * gNoiseWarpStrength;

    // Mid-frequency FBM terrain
    float baseHeight = TerrainFBM(warpedXZ * gNoiseScale);

    // Ridge noise for mountains
    float ridge = TerrainRidgeNoise(warpedXZ * gNoiseScale * 0.7);

    // Blend base and ridge based on another noise layer
    float ridgeMask = smoothstep(0.3, 0.7, TerrainFBM(worldXZ * gNoiseScale * 0.3));

    // Continental base modulates amplitude: flat in low areas, tall in high areas
    float detail = lerp(baseHeight, ridge, ridgeMask);
    float height = lerp(detail * 0.3, detail * 1.5, baseMask) * gNoiseHeightScale;

    // Clamp to stay within the normalization range used by height storage/DS
    return clamp(height, -gNoiseHeightScale, gNoiseHeightScale);
}

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

    // Generate height
    float height = ComputeHeight(worldXZ);

    // Compute normal via finite differences
    float eps = gNoiseCellSize;
    float hL = ComputeHeight(worldXZ + float2(-eps, 0));
    float hR = ComputeHeight(worldXZ + float2( eps, 0));
    float hD = ComputeHeight(worldXZ + float2(0, -eps));
    float hU = ComputeHeight(worldXZ + float2(0,  eps));

    float3 normal = normalize(float3(hL - hR, 2.0 * eps, hD - hU));
    float slope = 1.0 - normal.y; // 0=flat, 1=vertical

    // Compute material weights
    float4 weights = ComputeMaterialWeights(height, slope, worldXZ);

    // Pack normal (hemisphere encoding: normal is always pointing up)
    float2 packedNormal = normal.xz;

    // Write outputs
    // Store height normalized to [0, 1] for R16_FLOAT (height in [-HeightScale, HeightScale])
    gOutputHeightMap[DTid.xy] = height / (2.0 * gNoiseHeightScale) + 0.5;
    gOutputNormalMap[DTid.xy] = packedNormal;
    gOutputMaterialWeights[DTid.xy] = weights;
}
