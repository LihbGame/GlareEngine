#ifndef TERRAIN_NOISE_HLSLI
#define TERRAIN_NOISE_HLSLI

// Shared terrain noise functions for both CS generation and DS boundary evaluation.
// All functions are parameterized to avoid cbuffer name conflicts.

// --- Hash utilities ---
float TNoiseHash2D(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float2 TNoiseHash2D2(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.xx + p3.yz) * p3.zy);
}

// --- FBM with explicit parameters ---
float TerrainFBMImpl(float2 pos, uint octaves, float lacunarity, float persistence, float seedOffset)
{
    float value = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    float maxValue = 0.0;

    pos += seedOffset * 1000.0;

    float2x2 rot = { 0.8, 0.6, -0.6, 0.8 };

    [unroll]
    for (uint i = 0; i < 8; i++)
    {
        if (i >= octaves) break;
        value += amplitude * GradientNoise(pos * frequency);
        maxValue += amplitude;
        frequency *= lacunarity;
        amplitude *= persistence;
        pos = mul(rot, pos) + float2(1.7, 9.2);
    }

    return value / maxValue;
}

// --- Ridge noise with explicit parameters ---
float TerrainRidgeNoiseImpl(float2 pos, uint octaves, float lacunarity, float persistence, float seedOffset)
{
    float value = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    float maxValue = 0.0;

    pos += seedOffset * 800.0;

    float2x2 rot = { 0.8, 0.6, -0.6, 0.8 };

    [unroll]
    for (uint i = 0; i < 8; i++)
    {
        if (i >= octaves) break;
        float n = GradientNoise(pos * frequency);
        n = 1.0 - abs(n);
        n = n * n;
        value += amplitude * n;
        maxValue += amplitude;
        frequency *= lacunarity;
        amplitude *= persistence;
        pos = mul(rot, pos) + float2(5.3, 1.7);
    }

    return value / maxValue;
}

// --- Full height evaluation ---
float ComputeTerrainHeight(
    float2 worldXZ,
    float noiseScale,
    uint seed,
    uint octaves,
    float lacunarity,
    float persistence,
    float warpStrength,
    float warpScale,
    float heightScale)
{
    float seedOffset = TNoiseHash2D(float2(seed, seed * 1.37));
    float seedOffset2 = TNoiseHash2D(float2(seed * 2.17, seed * 0.73));

    // Low-frequency continental base
    float continental = TerrainFBMImpl(worldXZ * noiseScale * 0.1, octaves, lacunarity, persistence, seedOffset);
    float baseMask = smoothstep(0.2, 0.8, continental);

    // Domain warping
    float2 warp = float2(
        TerrainFBMImpl(worldXZ * warpScale + float2(0.0, 0.0), octaves, lacunarity, persistence, seedOffset),
        TerrainFBMImpl(worldXZ * warpScale + float2(5.2, 1.3), octaves, lacunarity, persistence, seedOffset)
    );
    float2 warpedXZ = worldXZ + warp * warpStrength;

    // Mid-frequency FBM terrain
    float baseHeight = TerrainFBMImpl(warpedXZ * noiseScale, octaves, lacunarity, persistence, seedOffset);

    // Ridge noise for mountains
    float ridge = TerrainRidgeNoiseImpl(warpedXZ * noiseScale * 0.7, octaves, lacunarity, persistence, seedOffset2);

    // Blend base and ridge
    float ridgeMask = smoothstep(0.3, 0.7, TerrainFBMImpl(worldXZ * noiseScale * 0.3, octaves, lacunarity, persistence, seedOffset));

    // Continental base modulates amplitude
    float detail = lerp(baseHeight, ridge, ridgeMask);
    float height = lerp(detail * 0.3, detail * 1.5, baseMask) * heightScale;

    return clamp(height, -heightScale, heightScale);
}

#endif // TERRAIN_NOISE_HLSLI
