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

    pos += seedOffset * 10.0;

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

    pos += seedOffset * 8.0;

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

// --- FBM with derivative modulation and analytical gradient ---
// Technique #2: derivative-guided modulation from iq's "Elevated" (1/(1+dot(d,d)))
// Technique #1: analytical gradient via chain rule through octave rotation
// Returns float3: .x = height value, .yz = gradient dh/d(input_x), dh/d(input_y)
// NOTE: Produces different values than TerrainFBMImpl due to derivative modulation.
//       Use ComputeTerrainHeightWithDerivatives (not ComputeTerrainHeight) to avoid mismatches.
float3 TerrainFBMDerivImpl(float2 pos, uint octaves, float lacunarity, float persistence, float seedOffset)
{
    pos += seedOffset * 10.0;

    const float2x2 rot = { 0.8, 0.6, -0.6, 0.8 };
    const float2x2 rotT = { 0.8, -0.6, 0.6, 0.8 }; // transpose = inverse for rotation

    float value = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    float maxValue = 0.0;

    float2 derivAccum = float2(0.0, 0.0); // accumulated derivative energy for modulation
    float2 gradient = float2(0.0, 0.0);    // analytical gradient w.r.t. input pos
    float2x2 jac = { 1.0, 0.0, 0.0, 1.0 }; // tracks (rot^T)^i for chain rule

    [unroll]
    for (uint i = 0; i < 8; i++)
    {
        if (i >= octaves) break;

        float3 n = GradientNoised(pos * frequency); // .x=value, .yz=d/d(arg)

        // Derivative-guided modulation: suppress detail where derivatives are high
        derivAccum += n.yz;
        float mod = 1.0 / (1.0 + dot(derivAccum, derivAccum));
        value += amplitude * n.x * mod;

        // Analytical gradient via chain rule: amp * freq * mod * (rot^T)^i * noise'
        // (first-order approximation: omits n.x * d(mod)/d(pos) which is small for smooth mod)
        gradient += amplitude * frequency * mod * mul(jac, n.yz);

        maxValue += amplitude;
        frequency *= lacunarity;
        amplitude *= persistence;
        pos = mul(rot, pos) + float2(1.7, 9.2);
        jac = mul(rotT, jac);
    }

    return float3(value / maxValue, gradient / maxValue);
}

// --- Ridge noise with derivative modulation and analytical gradient ---
float3 TerrainRidgeNoiseDerivImpl(float2 pos, uint octaves, float lacunarity, float persistence, float seedOffset)
{
    pos += seedOffset * 8.0;

    const float2x2 rot = { 0.8, 0.6, -0.6, 0.8 };
    const float2x2 rotT = { 0.8, -0.6, 0.6, 0.8 };

    float value = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    float maxValue = 0.0;

    float2 derivAccum = float2(0.0, 0.0);
    float2 gradient = float2(0.0, 0.0);
    float2x2 jac = { 1.0, 0.0, 0.0, 1.0 };

    [unroll]
    for (uint i = 0; i < 8; i++)
    {
        if (i >= octaves) break;

        float3 n = GradientNoised(pos * frequency);

        // Ridge: r = (1 - |n|)^2, d/dx = -2*(1-|n|)*sign(n)*n'
        float ridgeVal = 1.0 - abs(n.x);
        float ridgeDeriv = -2.0 * ridgeVal * sign(n.x);

        derivAccum += n.yz;
        float mod = 1.0 / (1.0 + dot(derivAccum, derivAccum));

        value += amplitude * ridgeVal * ridgeVal * mod;
        gradient += amplitude * frequency * ridgeDeriv * mod * mul(jac, n.yz);

        maxValue += amplitude;
        frequency *= lacunarity;
        amplitude *= persistence;
        pos = mul(rot, pos) + float2(5.3, 1.7);
        jac = mul(rotT, jac);
    }

    return float3(value / maxValue, gradient / maxValue);
}

// --- Full height evaluation with analytical gradient ---
// Returns float3: .x = height, .yz = gradient (dh/dworldX, dh/dworldZ)
float3 ComputeTerrainHeightWithDerivatives(
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

    // Continental base (value only, used as mask)
    float continental = TerrainFBMImpl(worldXZ * noiseScale * 0.1, octaves, lacunarity, persistence, seedOffset);
    float baseMask = smoothstep(0.2, 0.8, continental);

    // Domain warping with gradient tracking
    float3 warpX = TerrainFBMDerivImpl(worldXZ * warpScale, octaves, lacunarity, persistence, seedOffset);
    float3 warpZ = TerrainFBMDerivImpl(worldXZ * warpScale + float2(5.2, 1.3), octaves, lacunarity, persistence, seedOffset);
    float2 warp = float2(warpX.x, warpZ.x);
    float2 warpedXZ = worldXZ + warp * warpStrength;

    // Main terrain with gradient
    float3 terrain = TerrainFBMDerivImpl(warpedXZ * noiseScale, octaves, lacunarity, persistence, seedOffset);

    // Ridge noise with gradient
    float3 ridge = TerrainRidgeNoiseDerivImpl(warpedXZ * noiseScale * 0.7, octaves, lacunarity, persistence, seedOffset2);

    // Ridge mask (value only)
    float ridgeMask = smoothstep(0.3, 0.7, TerrainFBMImpl(worldXZ * noiseScale * 0.3, octaves, lacunarity, persistence, seedOffset));

    // Convert FBM gradients to w.r.t. warpedXZ (chain rule: multiply by noiseScale)
    float2 terrainGrad = terrain.yz * noiseScale;
    float2 ridgeGrad = ridge.yz * noiseScale * 0.7;

    // Blend height and gradient
    float detail = lerp(terrain.x, ridge.x, ridgeMask);
    float2 detailGrad = lerp(terrainGrad, ridgeGrad, ridgeMask);

    // Apply continental mask
    float height = lerp(detail * 0.3, detail * 1.5, baseMask) * heightScale;
    float2 grad = lerp(detailGrad * 0.3, detailGrad * 1.5, baseMask) * heightScale;

    // Chain rule through domain warping:
    // J = d(warpedXZ)/d(worldXZ) = I + warpStrength * warpScale * J_warp
    // gradient_wrt_worldXZ = J^T * gradient_wrt_warpedXZ
    float ws = warpStrength * warpScale;
    float2x2 chainJacT = float2x2(
        1.0 + ws * warpX.y, ws * warpZ.y,
        ws * warpX.z,        1.0 + ws * warpZ.z
    );
    grad = mul(chainJacT, grad);

    return float3(clamp(height, -heightScale, heightScale), grad);
}

#endif // TERRAIN_NOISE_HLSLI
