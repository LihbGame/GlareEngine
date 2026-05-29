#ifndef TERRAIN_NOISE_HLSLI
#define TERRAIN_NOISE_HLSLI

// Shared terrain noise functions for compute terrain generation.

#define TERRAIN_NOISE_BILLOW   0
#define TERRAIN_NOISE_GABOR    1
#define TERRAIN_NOISE_PERLIN   2
#define TERRAIN_NOISE_PHASOR   3
#define TERRAIN_NOISE_RIDGED   4
#define TERRAIN_NOISE_SIMPLEX  5
#define TERRAIN_NOISE_VALUE    6
#define TERRAIN_NOISE_VORONOI  7
#define TERRAIN_NOISE_WAVE     8
#define TERRAIN_NOISE_WHITE    9
#define TERRAIN_NOISE_WORLEY   10

#define TERRAIN_FRACTAL_SINGLE 0
#define TERRAIN_FRACTAL_FBM    1
#define TERRAIN_FRACTAL_RIDGED 2

#define TERRAIN_WARP_NONE      0
#define TERRAIN_WARP_FIXED     1
#define TERRAIN_WARP_RECURSIVE 2

#define TERRAIN_COMBINE_ADD      0
#define TERRAIN_COMBINE_MULTIPLY 1
#define TERRAIN_COMBINE_SUBTRACT 2
#define TERRAIN_COMBINE_MIN      3
#define TERRAIN_COMBINE_MAX      4
#define TERRAIN_COMBINE_BLEND    5

#define TERRAIN_PLACEMENT_WORLD    0
#define TERRAIN_PLACEMENT_ROTATED  1
#define TERRAIN_PLACEMENT_MIRRORED 2

#define TERRAIN_FILTER_NONE          0
#define TERRAIN_FILTER_SMOOTH        1
#define TERRAIN_FILTER_TERRACE       2
#define TERRAIN_FILTER_STRATA        3
#define TERRAIN_FILTER_DISTORTION    4
#define TERRAIN_FILTER_SEDIMENT_FILL 5

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

uint TerrainHashUint(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

uint TerrainHashInt2(int2 p)
{
    uint x = TerrainHashUint((uint)p.x);
    uint y = TerrainHashUint((uint)p.y + 0x9e3779b9u);
    return TerrainHashUint(x ^ y);
}

float TerrainHashToUnitFloat(uint h)
{
    return (float)(h & 0x00ffffffu) * (1.0 / 16777216.0);
}

float2 TerrainRandom22(int2 p)
{
    uint h = TerrainHashInt2(p);
    return float2(
        TerrainHashToUnitFloat(h),
        TerrainHashToUnitFloat(TerrainHashUint(h ^ 0x68bc21ebu)));
}

float2 TerrainRandomGradient(int2 p)
{
    return normalize(TerrainRandom22(p) * 2.0 - 1.0);
}

float TerrainGradientNoise(float2 p)
{
    int2 i = int2(floor(p));
    float2 f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);

    return lerp(lerp(dot(TerrainRandomGradient(i + int2(0, 0)), f - float2(0.0, 0.0)),
                     dot(TerrainRandomGradient(i + int2(1, 0)), f - float2(1.0, 0.0)), u.x),
                lerp(dot(TerrainRandomGradient(i + int2(0, 1)), f - float2(0.0, 1.0)),
                     dot(TerrainRandomGradient(i + int2(1, 1)), f - float2(1.0, 1.0)), u.x), u.y) * 1.65;
}

float3 TerrainGradientNoised(float2 p)
{
    int2 i = int2(floor(p));
    float2 f = frac(p);

    float2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float2 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);

    float2 ga = TerrainRandomGradient(i + int2(0, 0));
    float2 gb = TerrainRandomGradient(i + int2(1, 0));
    float2 gc = TerrainRandomGradient(i + int2(0, 1));
    float2 gd = TerrainRandomGradient(i + int2(1, 1));

    float va = dot(ga, f - float2(0.0, 0.0));
    float vb = dot(gb, f - float2(1.0, 0.0));
    float vc = dot(gc, f - float2(0.0, 1.0));
    float vd = dot(gd, f - float2(1.0, 1.0));

    float value = va + u.x * (vb - va) + u.y * (vc - va) + u.x * u.y * (va - vb - vc + vd);
    float2 gradient =
        ga + u.x * (gb - ga) + u.y * (gc - ga) + u.x * u.y * (ga - gb - gc + gd) +
        du * (u.yx * (va - vb - vc + vd) + float2(vb, vc) - va);

    return float3(value * 1.65, gradient * 1.65);
}

float TerrainValueNoise(float2 p)
{
    int2 i = int2(floor(p));
    float2 f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);

    float a = TNoiseHash2D((float2)(i + int2(0, 0)));
    float b = TNoiseHash2D((float2)(i + int2(1, 0)));
    float c = TNoiseHash2D((float2)(i + int2(0, 1)));
    float d = TNoiseHash2D((float2)(i + int2(1, 1)));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y) * 2.0 - 1.0;
}

float2 TerrainRotate(float2 p, float angle)
{
    float s, c;
    sincos(angle, s, c);
    return float2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float2 TerrainApplyPlacement(uint layer, float2 worldXZ)
{
    int placementMode = gNoiseLayerOptions[layer].z;
    float4 placement = gNoiseLayerPlacement[layer];
    float2 scale = max(abs(placement.zw), float2(0.0001, 0.0001));
    float2 p = worldXZ * scale + placement.xy;

    if (placementMode == TERRAIN_PLACEMENT_ROTATED)
    {
        p = TerrainRotate(p, gNoiseLayerWarp[layer].w);
    }
    else if (placementMode == TERRAIN_PLACEMENT_MIRRORED)
    {
        p = abs(p);
    }

    return p;
}

float2 TerrainApplyLayerWarp(uint layer, float2 placedWorld)
{
    int warpMode = gNoiseLayerOptions[layer].x;
    if (warpMode == TERRAIN_WARP_NONE)
    {
        return placedWorld;
    }

    float warpAmplitude = gNoiseLayerWarp[layer].y;
    float warpFrequency = max(gNoiseLayerWarp[layer].z, 0.0001);
    if (warpAmplitude <= 0.0001)
    {
        return placedWorld;
    }

    float2 p = placedWorld;
    float seed = (float)gNoiseSeed * 0.013 + (float)layer * 17.37;

    for (int i = 0; i < 3; ++i)
    {
        float2 warpPos = p * warpFrequency + float2(seed + 19.1 * i, seed * 1.7 + 7.3 * i);
        float2 warp = float2(
            TerrainGradientNoise(warpPos),
            TerrainGradientNoise(warpPos + float2(5.2, 1.3)));
        p += warp * (warpAmplitude / (1.0 + (float)i));

        if (warpMode == TERRAIN_WARP_FIXED)
        {
            break;
        }
    }

    return p;
}

float TerrainVoronoiNoise(float2 p, int mode)
{
    int2 cell = int2(floor(p));
    float2 f = frac(p);
    float f1 = 1000.0;
    float f2 = 1000.0;

    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            int2 neighbor = int2(x, y);
            float2 featurePoint = TerrainRandom22(cell + neighbor);
            float dist = length(float2(neighbor) + featurePoint - f);
            if (dist < f1)
            {
                f2 = f1;
                f1 = dist;
            }
            else
            {
                f2 = min(f2, dist);
            }
        }
    }

    if (mode == 1)
    {
        return saturate((f2 - f1) * 2.0) * 2.0 - 1.0;
    }

    return (1.0 - saturate(f1)) * 2.0 - 1.0;
}

float TerrainGaborApprox(float2 p)
{
    float result = 0.0;

    for (int i = 0; i < 4; ++i)
    {
        float2 dir = TerrainRandom22(int2(i * 17 + 3, i * 29 + 11)) * 2.0 - 1.0;
        dir = normalize(dir);
        float phase = TNoiseHash2D(float2((float)i, 31.7)) * PI2;
        result += sin(dot(p, dir) * PI2 + phase);
    }

    return result * 0.25;
}

float TerrainPhasorApprox(float2 p)
{
    float a = sin((p.x + p.y * 0.37) * PI2);
    float b = cos((p.y - p.x * 0.61) * PI2);
    return (a + b) * 0.5;
}

float TerrainEvaluateSingleNoise(int noiseType, float2 p, int voronoiMode)
{
    if (noiseType == TERRAIN_NOISE_BILLOW)
    {
        return abs(TerrainGradientNoise(p)) * 2.0 - 1.0;
    }
    if (noiseType == TERRAIN_NOISE_GABOR)
    {
        return TerrainGaborApprox(p);
    }
    if (noiseType == TERRAIN_NOISE_PHASOR)
    {
        return TerrainPhasorApprox(p);
    }
    if (noiseType == TERRAIN_NOISE_RIDGED)
    {
        return (1.0 - abs(TerrainGradientNoise(p))) * 2.0 - 1.0;
    }
    if (noiseType == TERRAIN_NOISE_SIMPLEX)
    {
        return TerrainGradientNoise(TerrainRotate(p, 0.523599) + float2(13.1, 7.7));
    }
    if (noiseType == TERRAIN_NOISE_VALUE)
    {
        return TerrainValueNoise(p);
    }
    if (noiseType == TERRAIN_NOISE_VORONOI || noiseType == TERRAIN_NOISE_WORLEY)
    {
        return TerrainVoronoiNoise(p, voronoiMode);
    }
    if (noiseType == TERRAIN_NOISE_WAVE)
    {
        return sin((p.x + p.y) * PI2);
    }
    if (noiseType == TERRAIN_NOISE_WHITE)
    {
        return TNoiseHash2D(p) * 2.0 - 1.0;
    }

    return TerrainGradientNoise(p);
}

float3 TerrainEvaluateSingleNoiseWithGradient(int noiseType, float2 p, int voronoiMode)
{
    if (noiseType == TERRAIN_NOISE_BILLOW)
    {
        float3 n = TerrainGradientNoised(p);
        return float3(abs(n.x) * 2.0 - 1.0, sign(n.x) * n.yz * 2.0);
    }
    if (noiseType == TERRAIN_NOISE_RIDGED)
    {
        float3 n = TerrainGradientNoised(p);
        return float3((1.0 - abs(n.x)) * 2.0 - 1.0, -sign(n.x) * n.yz * 2.0);
    }
    if (noiseType == TERRAIN_NOISE_SIMPLEX)
    {
        float3 n = TerrainGradientNoised(TerrainRotate(p, 0.523599) + float2(13.1, 7.7));
        float2 grad = TerrainRotate(n.yz, 0.523599);
        return float3(n.x, grad);
    }
    if (noiseType == TERRAIN_NOISE_WAVE)
    {
        float phase = (p.x + p.y) * PI2;
        float c = cos(phase) * PI2;
        return float3(sin(phase), float2(c, c));
    }
    if (noiseType == TERRAIN_NOISE_PERLIN)
    {
        return TerrainGradientNoised(p);
    }

    return float3(TerrainEvaluateSingleNoise(noiseType, p, voronoiMode), 0.0, 0.0);
}

float3 EvaluateTerrainNoiseLayerWithGradient(uint layer, float2 worldXZ)
{
    int4 controls = gNoiseLayerControls[layer];
    int4 options = gNoiseLayerOptions[layer];
    float4 shape = gNoiseLayerShape[layer];
    float4 warp = gNoiseLayerWarp[layer];

    float frequency = max(shape.z, 0.0001);
    float lacunarity = max(shape.w, 1.0001);
    float gain = saturate(warp.x);
    uint octaves = (uint)clamp(options.y, 1, 8);
    float seedOffset = (float)gNoiseSeed * 0.071 + (float)layer * 23.13;

    float2 placed = TerrainApplyPlacement(layer, worldXZ);
    float2 warped = TerrainApplyLayerWarp(layer, placed);
    float2 p = warped * frequency + seedOffset;
    float2 dWorldX = float2(gNoiseLayerPlacement[layer].z, 0.0);
    float2 dWorldZ = float2(0.0, gNoiseLayerPlacement[layer].w);
    if (options.z == TERRAIN_PLACEMENT_ROTATED)
    {
        dWorldX = TerrainRotate(dWorldX, warp.w);
        dWorldZ = TerrainRotate(dWorldZ, warp.w);
    }
    else if (options.z == TERRAIN_PLACEMENT_MIRRORED)
    {
        float2 mirrorSign = lerp(float2(-1.0, -1.0), float2(1.0, 1.0), step(0.0, placed));
        dWorldX *= mirrorSign;
        dWorldZ *= mirrorSign;
    }
    dWorldX *= frequency;
    dWorldZ *= frequency;

    if (controls.z == TERRAIN_FRACTAL_SINGLE)
    {
        float3 n = TerrainEvaluateSingleNoiseWithGradient(controls.y, p, options.w);
        float2 worldGrad = float2(dot(n.yz, dWorldX), dot(n.yz, dWorldZ));
        return float3(n.x * shape.y, worldGrad * shape.y);
    }

    float value = 0.0;
    float2 gradient = 0.0;
    float amplitude = 1.0;
    float maxValue = 0.0;
    float2 octavePos = p;
    const float2x2 rot = { 0.8, 0.6, -0.6, 0.8 };
    const float2x2 rotT = { 0.8, -0.6, 0.6, 0.8 };
    float2x2 jac = { 1.0, 0.0, 0.0, 1.0 };

    for (uint octave = 0; octave < 8; ++octave)
    {
        if (octave >= octaves)
        {
            break;
        }

        float3 n = TerrainEvaluateSingleNoiseWithGradient(controls.y, octavePos, options.w);
        if (controls.z == TERRAIN_FRACTAL_RIDGED)
        {
            n = float3((1.0 - abs(n.x)) * 2.0 - 1.0, -sign(n.x) * n.yz * 2.0);
        }

        value += n.x * amplitude;
        gradient += mul(jac, n.yz) * amplitude;
        maxValue += amplitude;
        amplitude *= gain;
        octavePos = mul(rot, octavePos * lacunarity) + float2(1.7, 9.2);
        jac = mul(rotT * lacunarity, jac);
    }

    float invMaxValue = 1.0 / max(maxValue, 0.0001);
    gradient *= invMaxValue;
    float2 worldGrad = float2(dot(gradient, dWorldX), dot(gradient, dWorldZ));
    return float3(value * invMaxValue * shape.y, worldGrad * shape.y);
}

float EvaluateTerrainNoiseLayer(uint layer, float2 worldXZ)
{
    return EvaluateTerrainNoiseLayerWithGradient(layer, worldXZ).x;
}

float TerrainCombineLayerValue(float currentValue, float layerValue, float opacity, int op, bool hasValue)
{
    if (!hasValue)
    {
        return layerValue * opacity;
    }

    if (op == TERRAIN_COMBINE_MULTIPLY)
    {
        return lerp(currentValue, currentValue * (1.0 + layerValue), opacity);
    }
    if (op == TERRAIN_COMBINE_SUBTRACT)
    {
        return currentValue - layerValue * opacity;
    }
    if (op == TERRAIN_COMBINE_MIN)
    {
        return lerp(currentValue, min(currentValue, layerValue), opacity);
    }
    if (op == TERRAIN_COMBINE_MAX)
    {
        return lerp(currentValue, max(currentValue, layerValue), opacity);
    }
    if (op == TERRAIN_COMBINE_BLEND)
    {
        return lerp(currentValue, layerValue, opacity);
    }

    return currentValue + layerValue * opacity;
}

float3 TerrainCombineLayerResult(float3 currentResult, float3 layerResult, float opacity, int op, bool hasValue)
{
    if (!hasValue)
    {
        return layerResult * opacity;
    }

    if (op == TERRAIN_COMBINE_MULTIPLY)
    {
        float value = currentResult.x * (1.0 + layerResult.x);
        float2 gradient = currentResult.yz * (1.0 + layerResult.x) + currentResult.x * layerResult.yz;
        return lerp(currentResult, float3(value, gradient), opacity);
    }
    if (op == TERRAIN_COMBINE_SUBTRACT)
    {
        return currentResult - layerResult * opacity;
    }
    if (op == TERRAIN_COMBINE_MIN)
    {
        return layerResult.x < currentResult.x ? lerp(currentResult, layerResult, opacity) : currentResult;
    }
    if (op == TERRAIN_COMBINE_MAX)
    {
        return layerResult.x > currentResult.x ? lerp(currentResult, layerResult, opacity) : currentResult;
    }
    if (op == TERRAIN_COMBINE_BLEND)
    {
        return lerp(currentResult, layerResult, opacity);
    }

    return currentResult + layerResult * opacity;
}

float3 EvaluateNoiseStackWithGradient(uint startLayer, uint layerCount, float2 worldXZ)
{
    float3 result = 0.0;
    bool hasValue = false;

    for (uint i = 0; i < 4; ++i)
    {
        if (i >= layerCount)
        {
            break;
        }

        uint layer = startLayer + i;
        if (gNoiseLayerControls[layer].x == 0)
        {
            continue;
        }

        float opacity = saturate(gNoiseLayerShape[layer].x);
        float3 layerResult = EvaluateTerrainNoiseLayerWithGradient(layer, worldXZ);
        result = TerrainCombineLayerResult(result, layerResult, opacity, gNoiseLayerControls[layer].w, hasValue);
        hasValue = true;
    }

    return result;
}

float EvaluateNoiseStack(uint startLayer, uint layerCount, float2 worldXZ)
{
    return EvaluateNoiseStackWithGradient(startLayer, layerCount, worldXZ).x;
}

float3 ComputeTerrainRawHeightWithDerivatives(
    float2 worldXZ,
    float heightScale)
{
    uint baseCount = (uint)clamp(gNoiseLayerCounts.x, 0, 4);
    uint detailCount = (uint)clamp(gNoiseLayerCounts.y, 0, 4);

    float3 baseResult = EvaluateNoiseStackWithGradient(0, baseCount, worldXZ);
    float3 detailResult = EvaluateNoiseStackWithGradient(4, detailCount, worldXZ);
    float3 result = (baseResult + detailResult) * heightScale;
    result.x = clamp(result.x, -heightScale, heightScale);
    return result;
}

float ComputeTerrainRawHeight(float2 worldXZ, float heightScale)
{
    uint baseCount = (uint)clamp(gNoiseLayerCounts.x, 0, 4);
    uint detailCount = (uint)clamp(gNoiseLayerCounts.y, 0, 4);

    float baseValue = EvaluateNoiseStack(0, baseCount, worldXZ);
    float detailValue = EvaluateNoiseStack(4, detailCount, worldXZ);
    float height = (baseValue + detailValue) * heightScale;

    return clamp(height, -heightScale, heightScale);
}

float TerrainCombineFilterHeight(float currentHeight, float candidateHeight, float strength, int op, float heightScale)
{
    strength = saturate(strength);

    if (op == TERRAIN_COMBINE_SUBTRACT)
    {
        return currentHeight - (candidateHeight - currentHeight) * strength;
    }
    if (op == TERRAIN_COMBINE_MIN)
    {
        return lerp(currentHeight, min(currentHeight, candidateHeight), strength);
    }
    if (op == TERRAIN_COMBINE_MAX)
    {
        return lerp(currentHeight, max(currentHeight, candidateHeight), strength);
    }
    if (op == TERRAIN_COMBINE_MULTIPLY)
    {
        float normalizedDelta = (candidateHeight - currentHeight) / max(heightScale, 1.0);
        return lerp(currentHeight, currentHeight * (1.0 + normalizedDelta), strength);
    }

    return lerp(currentHeight, candidateHeight, strength);
}

float TerrainApplySmoothFilter(float2 worldXZ, float currentHeight, float radius, int iterations, float heightScale)
{
    float height = currentHeight;
    float sampleRadius = max(radius, gNoiseCellSize);
    int iterationCount = clamp(iterations, 1, 4);

    for (int i = 0; i < 4; ++i)
    {
        if (i >= iterationCount)
        {
            break;
        }

        float hL = ComputeTerrainRawHeight(worldXZ + float2(-sampleRadius, 0.0), heightScale);
        float hR = ComputeTerrainRawHeight(worldXZ + float2(sampleRadius, 0.0), heightScale);
        float hD = ComputeTerrainRawHeight(worldXZ + float2(0.0, -sampleRadius), heightScale);
        float hU = ComputeTerrainRawHeight(worldXZ + float2(0.0, sampleRadius), heightScale);
        float hLD = ComputeTerrainRawHeight(worldXZ + float2(-sampleRadius, -sampleRadius), heightScale);
        float hRU = ComputeTerrainRawHeight(worldXZ + float2(sampleRadius, sampleRadius), heightScale);
        float hLU = ComputeTerrainRawHeight(worldXZ + float2(-sampleRadius, sampleRadius), heightScale);
        float hRD = ComputeTerrainRawHeight(worldXZ + float2(sampleRadius, -sampleRadius), heightScale);

        height = (height + hL + hR + hD + hU + hLD + hRU + hLU + hRD) / 9.0;
        sampleRadius *= 1.5;
    }

    return height;
}

float TerrainApplyTerraceFilter(float currentHeight, float stepHeight, float smoothness, float offset)
{
    float stepSize = max(stepHeight, 0.001);
    float terracePosition = (currentHeight + offset) / stepSize;
    float terraceBase = floor(terracePosition);
    float terraceLocal = frac(terracePosition);
    float transition = smoothstep(0.0, max(smoothness, 0.001), terraceLocal);
    return (terraceBase + transition) * stepSize - offset;
}

float TerrainApplyStrataFilter(float2 worldXZ, float currentHeight, float spacing, float amplitude, float tilt, float angle)
{
    float2 dir = float2(cos(angle), sin(angle));
    float bandCoord = (dot(worldXZ, dir) + currentHeight * tilt) / max(spacing, 0.001);
    float band = sin(bandCoord * PI2);
    float sharpenedBand = sign(band) * pow(abs(band), 0.65);
    return currentHeight + sharpenedBand * amplitude;
}

float TerrainApplyDistortionFilter(float2 worldXZ, float radius, float warpFrequency, float warpAmplitude, float heightScale)
{
    float2 warpPos = worldXZ * max(warpFrequency, 0.00001) + (float)gNoiseSeed * 0.017;
    float2 warp = float2(
        TerrainGradientNoise(warpPos),
        TerrainGradientNoise(warpPos + float2(17.3, 5.1)));
    float2 distortedXZ = worldXZ + warp * radius * saturate(warpAmplitude);
    return ComputeTerrainRawHeight(distortedXZ, heightScale);
}

float TerrainApplySedimentFillFilter(float2 worldXZ, float currentHeight, float radius, float fillLimit, int iterations, float heightScale)
{
    float height = currentHeight;
    float sampleRadius = max(radius, gNoiseCellSize);
    int iterationCount = clamp(iterations, 1, 4);

    for (int i = 0; i < 4; ++i)
    {
        if (i >= iterationCount)
        {
            break;
        }

        float hL = ComputeTerrainRawHeight(worldXZ + float2(-sampleRadius, 0.0), heightScale);
        float hR = ComputeTerrainRawHeight(worldXZ + float2(sampleRadius, 0.0), heightScale);
        float hD = ComputeTerrainRawHeight(worldXZ + float2(0.0, -sampleRadius), heightScale);
        float hU = ComputeTerrainRawHeight(worldXZ + float2(0.0, sampleRadius), heightScale);
        float neighborhood = (hL + hR + hD + hU) * 0.25;
        float fill = min(max(neighborhood - height, 0.0), max(fillLimit, 0.0));
        height += fill;
        sampleRadius *= 1.5;
    }

    return height;
}

float ApplyTerrainFilterStack(float2 worldXZ, float rawHeight, float heightScale)
{
    float height = rawHeight;
    uint filterCount = (uint)clamp(gTerrainFilterCounts.x, 0, 4);

    for (uint i = 0; i < 4; ++i)
    {
        if (i >= filterCount)
        {
            break;
        }

        int4 controls = gTerrainFilterControls[i];
        if (controls.x == 0 || controls.y == TERRAIN_FILTER_NONE)
        {
            continue;
        }

        float4 params0 = gTerrainFilterParams0[i];
        float4 params1 = gTerrainFilterParams1[i];
        float strength = params0.x;
        float radius = max(params0.y, gNoiseCellSize);
        int iterations = controls.w;
        float candidate = height;

        if (controls.y == TERRAIN_FILTER_SMOOTH)
        {
            candidate = TerrainApplySmoothFilter(worldXZ, height, radius, iterations, heightScale);
        }
        else if (controls.y == TERRAIN_FILTER_TERRACE)
        {
            candidate = TerrainApplyTerraceFilter(height, params0.z, params0.w, params1.x);
        }
        else if (controls.y == TERRAIN_FILTER_STRATA)
        {
            candidate = TerrainApplyStrataFilter(worldXZ, height, params0.z, params0.w, params1.x, params1.y);
        }
        else if (controls.y == TERRAIN_FILTER_DISTORTION)
        {
            candidate = TerrainApplyDistortionFilter(worldXZ, radius, params0.z, params0.w, heightScale);
        }
        else if (controls.y == TERRAIN_FILTER_SEDIMENT_FILL)
        {
            candidate = TerrainApplySedimentFillFilter(worldXZ, height, radius, params0.z, iterations, heightScale);
        }

        height = TerrainCombineFilterHeight(height, candidate, strength, controls.z, heightScale);
        height = clamp(height, -heightScale, heightScale);
    }

    return height;
}

float ComputeTerrainHeight(
    float2 worldXZ,
    float heightScale)
{
    float rawHeight = ComputeTerrainRawHeight(worldXZ, heightScale);
    return ApplyTerrainFilterStack(worldXZ, rawHeight, heightScale);
}

float3 ComputeTerrainHeightWithDerivatives(
    float2 worldXZ,
    float heightScale)
{
    float3 rawResult = ComputeTerrainRawHeightWithDerivatives(worldXZ, heightScale);
    rawResult.x = ApplyTerrainFilterStack(worldXZ, rawResult.x, heightScale);
    return rawResult;
}

#endif // TERRAIN_NOISE_HLSLI
