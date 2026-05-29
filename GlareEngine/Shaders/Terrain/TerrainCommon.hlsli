#ifndef TERRAIN_COMMON_HLSLI
#define TERRAIN_COMMON_HLSLI

#include "../Misc/CommonResource.hlsli"

#define TERRAIN_CLIPMAP_LEVELS  10
#define TERRAIN_TILE_SIZE       64
#define TERRAIN_HEIGHTMAP_SIZE  256
#define TERRAIN_NUM_LAYERS      5

// --- Clipmap vertex structures ---

struct ClipmapVertexIn
{
    float3  Position    : POSITION;    // tile-local grid position (x,z in world, y=0)
    float2  Tex         : TEXCOORD;    // tile UV [0,1]
    float2  BoundsY     : BOUNDY;      // min/max height bounds for patch
};

struct ClipmapVertexOut
{
    float3  PosW        : POSITION;
    float2  TileUV      : TEXCOORD0;
    float2  BoundsY     : BOUNDY;
    uint    LODLevel    : LODLEVEL;
    int2    TileOffset  : TILEOFFSET;
};

// --- Tessellation structures ---

struct ClipmapPatchTess
{
    float EdgeTess[4]   : SV_TessFactor;
    float InsideTess[2] : SV_InsideTessFactor;
};

struct ClipmapHullOut
{
    float3  PosW        : POSITION;
    float2  TileUV      : TEXCOORD0;
    uint    LODLevel    : LODLEVEL;
    int2    TileOffset  : TILEOFFSET;
};

struct ClipmapDomainOut
{
    float4  PosH        : SV_POSITION;
    float3  PosW        : POSITION0;
    float3  NormalW     : NORMAL;
    float3  TangentW    : TANGENT;
    float2  TileUV      : TEXCOORD0;
    float2  WorldXZ     : TEXCOORD1;
    float4  MatWeights  : TEXCOORD2;   // 5th weight = 1 - sum
    float3  CurPosition : TEXCOORD3;
    float3  PrePosition : TEXCOORD4;
};

// --- Terrain render constant buffer ---

cbuffer ProceduralTerrainCB : register(b1)
{
    float4x4    gTerrainViewProj;
    float3      gTerrainEyePosW;
    float       gTerrainMinTessDist;
    float       gTerrainMaxTessDist;
    float       gTerrainMinTess;
    float       gTerrainMaxTess;
    float       gTerrainTessScale;
    int         gClipmapLevel;
    float       gTerrainCellSize;
    float       gTerrainHeightScale;
    float       gTerrainTexScale;
    float       gStochasticSharpness;
    float3      _PadTessScale;
    float4      gTerrainFrustumPlanes[6];
    int         gTerrainHeightMapIndex;
    int         gTerrainNormalMapIndex;
    int         gTerrainWeightMapIndex;
    int         gTerrainTileOffsetX;     // grid coord * TileSize
    int         gTerrainTileOffsetY;     // grid coord * TileSize
    int4        gTerrainLayerAlbedo[5];
    int4        gTerrainLayerNormal[5];
    int4        gTerrainLayerRoughness[5];
    int4        gTerrainLayerMetallic[5];
    int4        gTerrainLayerAO[5];
    // Finer LOD level coverage bounds for patch-level clipping
    int         gHasFinerLevel;
    float       gFinerLevelMinX;
    float       gFinerLevelMaxX;
    float       gFinerLevelMinZ;
    float       gFinerLevelMaxZ;
    float       gTerrainRoughnessScale;
    float       gTerrainMetallicScale;
    float       gTerrainSkirtDepth;
    uint        gTerrainSkirtEdgeFlags;
    float2      _PadMV;
    // Previous frame data for motion vector computation
    float4x4    gTerrainPreViewProj;
    float2      gTerrainPreJitter;
    float2      gTerrainCurJitter;
    // Detail texture parameters
    float       gDetailScale;
    float       gDetailFadeDistance;
    int         gTerrainUseHeightBlend;
    int         gTerrainUseTriplanar;
    float       gTerrainParallaxHeightScale;
    int         gTerrainUseParallax;
    float2      _PadParallax;
    int4        gDetailLayerAlbedo[5];
    int4        gDetailLayerNormal[5];
    int4        gDetailLayerRoughness[5];
    int4        gTerrainLayerHeight[5];
};

// --- Noise generation constant buffer ---

cbuffer TerrainNoiseCB : register(b0)
{
    float3  gNoiseCameraPos;
    float   gNoiseCellSize;
    int2    gNoiseTileOffset;
    int     gNoiseTileSize;
    int     gNoiseHeightmapSize;
    float   gNoiseHeightScale;
    uint    gNoiseSeed;
    float   gNoisePadHeader0;
    float   gNoisePadHeader1;
    float   gNoisePadHeader2;
    float   gNoisePadHeader3;
    float   gNoisePadHeader4;
    float   gNoisePadHeader5;
    int4    gNoiseLayerControls[8];  // enabled, noise type, fractal type, combine op
    int4    gNoiseLayerOptions[8];   // warp mode, octaves, placement mode, voronoi mode
    float4  gNoiseLayerShape[8];     // opacity, amplitude, frequency, lacunarity
    float4  gNoiseLayerWarp[8];      // gain, warp amplitude, warp frequency, rotation
    float4  gNoiseLayerPlacement[8]; // offset xz, scale xz
    int4    gNoiseLayerCounts;       // base count, detail count, reserved

    float   gNoiseSnowHeight;
    float   gNoiseSnowTransition;
    float   gNoiseStoneSlope;
    float   gNoiseStoneTransition;
    int     gNoiseLODLevel;
    int     gNoisePadFooter0;
    float   gNoisePad1;
    float   gNoisePad2;
};

// --- Stochastic sampling utility ---

float2x2 StochasticRotation(float2 worldPos)
{
    // random22 scrambles both axes and avoids directional streaks from random12's 1D projection.
    float angle = random22(worldPos * 0.137).x * PI2;
    float s, c;
    sincos(angle, s, c);
    return float2x2(c, -s, s, c);
}

float3 SampleStochastic(int srvIndex, float2 baseUV, float2 worldPos)
{
    float2x2 rot = StochasticRotation(worldPos);

    float2 uv1 = baseUV;
    float2 uv2 = mul(rot, baseUV - 0.5) + 0.5;

    float3 s1 = gSRVMap[srvIndex].Sample(gSamplerAnisoWrap, uv1).rgb;
    float3 s2 = gSRVMap[srvIndex].Sample(gSamplerAnisoWrap, uv2).rgb;

    // Decorrelated 2D hash avoids streak artifacts from 1D-projected random12.
    float blend = random22(worldPos * float2(1.23, 0.58) + 17.3).x;
    blend = smoothstep(0.5 - gStochasticSharpness * 0.5,
                       0.5 + gStochasticSharpness * 0.5, blend);

    return lerp(s1, s2, blend);
}

float SampleStochasticScalar(int srvIndex, float2 baseUV, float2 worldPos)
{
    float2x2 rot = StochasticRotation(worldPos);

    float2 uv1 = baseUV;
    float2 uv2 = mul(rot, baseUV - 0.5) + 0.5;

    float s1 = gSRVMap[srvIndex].Sample(gSamplerAnisoWrap, uv1).r;
    float s2 = gSRVMap[srvIndex].Sample(gSamplerAnisoWrap, uv2).r;

    float blend = random22(worldPos * float2(1.23, 0.58) + 17.3).x;
    blend = smoothstep(0.5 - gStochasticSharpness * 0.5,
                       0.5 + gStochasticSharpness * 0.5, blend);

    return lerp(s1, s2, blend);
}

float3 SampleStochasticNormal(int srvIndex, float2 baseUV, float2 worldPos)
{
    float2x2 rot = StochasticRotation(worldPos);

    float2 uv1 = baseUV;
    float2 uv2 = mul(rot, baseUV - 0.5) + 0.5;

    float3 s1 = gSRVMap[srvIndex].Sample(gSamplerAnisoWrap, uv1).rgb;
    float3 s2 = gSRVMap[srvIndex].Sample(gSamplerAnisoWrap, uv2).rgb;

    float blend = random22(worldPos * float2(1.23, 0.58) + 17.3).x;
    blend = smoothstep(0.5 - gStochasticSharpness * 0.5,
                       0.5 + gStochasticSharpness * 0.5, blend);

    return lerp(s1, s2, blend);
}

float2 GetStableStochasticOffset(float2 cell, float seed)
{
    return (random22(cell + seed) - 0.5) * saturate(gStochasticSharpness);
}

float2 GetStableStochasticWeights(float2 baseUV)
{
    float2 f = frac(baseUV);
    return f * f * (3.0 - 2.0 * f);
}

float3 SampleStableStochastic(int srvIndex, float2 baseUV, float2 uvDx, float2 uvDy)
{
    if (gStochasticSharpness <= 0.001)
    {
        return gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, baseUV, uvDx, uvDy).rgb;
    }

    float2 cell = floor(baseUV);
    float2 w = GetStableStochasticWeights(baseUV);

    float3 s00 = gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, baseUV + GetStableStochasticOffset(cell, 11.0), uvDx, uvDy).rgb;
    float3 s10 = gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, baseUV + GetStableStochasticOffset(cell + float2(1.0, 0.0), 11.0), uvDx, uvDy).rgb;
    float3 s01 = gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, baseUV + GetStableStochasticOffset(cell + float2(0.0, 1.0), 11.0), uvDx, uvDy).rgb;
    float3 s11 = gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, baseUV + GetStableStochasticOffset(cell + float2(1.0, 1.0), 11.0), uvDx, uvDy).rgb;

    return lerp(lerp(s00, s10, w.x), lerp(s01, s11, w.x), w.y);
}

float SampleStableStochasticScalar(int srvIndex, float2 baseUV, float2 uvDx, float2 uvDy)
{
    if (gStochasticSharpness <= 0.001)
    {
        return gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, baseUV, uvDx, uvDy).r;
    }

    float2 cell = floor(baseUV);
    float2 w = GetStableStochasticWeights(baseUV);

    float s00 = gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, baseUV + GetStableStochasticOffset(cell, 23.0), uvDx, uvDy).r;
    float s10 = gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, baseUV + GetStableStochasticOffset(cell + float2(1.0, 0.0), 23.0), uvDx, uvDy).r;
    float s01 = gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, baseUV + GetStableStochasticOffset(cell + float2(0.0, 1.0), 23.0), uvDx, uvDy).r;
    float s11 = gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, baseUV + GetStableStochasticOffset(cell + float2(1.0, 1.0), 23.0), uvDx, uvDy).r;

    return lerp(lerp(s00, s10, w.x), lerp(s01, s11, w.x), w.y);
}

// --- Terrain material projection utilities ---

#define TERRAIN_TRIPLANAR_FULL_SLOPE_Y  0.35
#define TERRAIN_TRIPLANAR_START_SLOPE_Y 0.72
#define TERRAIN_TRIPLANAR_BLEND_POWER   4.0

float TerrainTriplanarFade(float3 normalW)
{
    float upness = abs(normalize(normalW).y);
    float fade = saturate((TERRAIN_TRIPLANAR_START_SLOPE_Y - upness) /
        max(TERRAIN_TRIPLANAR_START_SLOPE_Y - TERRAIN_TRIPLANAR_FULL_SLOPE_Y, 0.001));
    return fade * fade * (3.0 - 2.0 * fade);
}

float3 TerrainTriplanarWeights(float3 normalW)
{
    float3 w = pow(max(abs(normalize(normalW)), 0.0001), TERRAIN_TRIPLANAR_BLEND_POWER);
    return w / max(w.x + w.y + w.z, 0.0001);
}

float3 TerrainDecodeNormal(float3 normalSample)
{
    return normalize(normalSample * 2.0 - 1.0);
}

float3 TerrainPlanarNormalToWorld(float3 normalSample, float3x3 tbn)
{
    return normalize(mul(TerrainDecodeNormal(normalSample), tbn));
}

float3 TerrainTriplanarNormalToWorld(float3 normalSample, uint axis, float axisSign)
{
    float3 n = TerrainDecodeNormal(normalSample);

    if (axis == 0)
    {
        return normalize(float3(n.z * axisSign, n.y, n.x));
    }
    if (axis == 1)
    {
        return normalize(float3(n.x, n.z * axisSign, n.y));
    }

    return normalize(float3(n.x, n.y, n.z * axisSign));
}

struct TerrainTriplanarUVSet
{
    float2 UvX;
    float2 UvY;
    float2 UvZ;
    float2 DxX;
    float2 DxY;
    float2 DxZ;
    float2 DyX;
    float2 DyY;
    float2 DyZ;
    float3 Weights;
    float3 SignN;
};

TerrainTriplanarUVSet TerrainBuildTriplanarUVSet(float3 posW, float3 posDx, float3 posDy, float3 normalW, float scale)
{
    TerrainTriplanarUVSet uvSet;
    float invScale = 1.0 / max(scale, 0.001);
    float3 dxPos = posDx * invScale;
    float3 dyPos = posDy * invScale;

    // X projects onto ZY, Y onto XZ, Z onto XY.
    uvSet.UvX = posW.zy * invScale;
    uvSet.UvY = posW.xz * invScale;
    uvSet.UvZ = posW.xy * invScale;

    uvSet.DxX = dxPos.zy;
    uvSet.DxY = dxPos.xz;
    uvSet.DxZ = dxPos.xy;

    uvSet.DyX = dyPos.zy;
    uvSet.DyY = dyPos.xz;
    uvSet.DyZ = dyPos.xy;

    uvSet.Weights = TerrainTriplanarWeights(normalW);
    uvSet.SignN = float3(
        normalW.x >= 0.0 ? 1.0 : -1.0,
        normalW.y >= 0.0 ? 1.0 : -1.0,
        normalW.z >= 0.0 ? 1.0 : -1.0);

    return uvSet;
}

float3 TerrainSamplePlanarRGB(int srvIndex, float2 uv, float2 uvDx, float2 uvDy, bool alignedSampling)
{
    return alignedSampling
        ? gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, uv, uvDx, uvDy).rgb
        : SampleStableStochastic(srvIndex, uv, uvDx, uvDy);
}

float TerrainSamplePlanarScalar(int srvIndex, float2 uv, float2 uvDx, float2 uvDy, bool alignedSampling)
{
    return alignedSampling
        ? gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, uv, uvDx, uvDy).r
        : SampleStableStochasticScalar(srvIndex, uv, uvDx, uvDy);
}

float3 TerrainSampleTriplanarRGB(int srvIndex, TerrainTriplanarUVSet uvSet)
{
    float3 sx = gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, uvSet.UvX, uvSet.DxX, uvSet.DyX).rgb;
    float3 sy = gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, uvSet.UvY, uvSet.DxY, uvSet.DyY).rgb;
    float3 sz = gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, uvSet.UvZ, uvSet.DxZ, uvSet.DyZ).rgb;

    return sx * uvSet.Weights.x + sy * uvSet.Weights.y + sz * uvSet.Weights.z;
}

float TerrainSampleTriplanarScalar(int srvIndex, TerrainTriplanarUVSet uvSet)
{
    float sx = gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, uvSet.UvX, uvSet.DxX, uvSet.DyX).r;
    float sy = gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, uvSet.UvY, uvSet.DxY, uvSet.DyY).r;
    float sz = gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, uvSet.UvZ, uvSet.DxZ, uvSet.DyZ).r;

    return sx * uvSet.Weights.x + sy * uvSet.Weights.y + sz * uvSet.Weights.z;
}

float3 TerrainSampleTriplanarNormalW(int srvIndex, TerrainTriplanarUVSet uvSet)
{
    float3 nx = TerrainTriplanarNormalToWorld(gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, uvSet.UvX, uvSet.DxX, uvSet.DyX).rgb, 0, uvSet.SignN.x);
    float3 ny = TerrainTriplanarNormalToWorld(gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, uvSet.UvY, uvSet.DxY, uvSet.DyY).rgb, 1, uvSet.SignN.y);
    float3 nz = TerrainTriplanarNormalToWorld(gSRVMap[srvIndex].SampleGrad(gSamplerAnisoWrap, uvSet.UvZ, uvSet.DxZ, uvSet.DyZ).rgb, 2, uvSet.SignN.z);

    return normalize(nx * uvSet.Weights.x + ny * uvSet.Weights.y + nz * uvSet.Weights.z);
}

// --- Clipmap tessellation factor ---

float CalcClipmapTessFactor(float3 p)
{
    float d = distance(p, gTerrainEyePosW);
    float s = saturate((d - gTerrainMinTessDist) / (gTerrainMaxTessDist - gTerrainMinTessDist));
    float tess = lerp(gTerrainMaxTess, gTerrainMinTess, s);
    tess = max(tess, gTerrainMinTess);
    return tess;
}

// --- Bicubic Catmull-Rom sampling ---

// Convert tile UV [0,1] to heightmap UV for correct texel lookup
float2 TileToHeightmapUV(float2 tileUV)
{
    return (tileUV * (float)(TERRAIN_HEIGHTMAP_SIZE - 1) + 0.5) / (float)TERRAIN_HEIGHTMAP_SIZE;
}

float SampleHeightBicubic(float2 tileUV)
{
    float2 hmUV = TileToHeightmapUV(tileUV);
    float texSize = (float)TERRAIN_HEIGHTMAP_SIZE;
    float invTexSize = 1.0 / texSize;

    float2 pixelPos = hmUV * texSize - 0.5;
    float2 base = floor(pixelPos);
    float2 t = pixelPos - base;
    float2 t2 = t * t;
    float2 t3 = t2 * t;

    // Catmull-Rom weights (t in [0,1], interpolates between P1 and P2)
    float2 w0 = (-t3 + 2.0 * t2 - t) * 0.5;
    float2 w1 = (3.0 * t3 - 5.0 * t2 + 2.0) * 0.5;
    float2 w2 = (-3.0 * t3 + 4.0 * t2 + t) * 0.5;
    float2 w3 = (t3 - t2) * 0.5;

    float4 wy = float4(w0.y, w1.y, w2.y, w3.y);
    float4 wx = float4(w0.x, w1.x, w2.x, w3.x);

    float result = 0.0;
    [unroll]
    for (int j = 0; j < 4; j++)
    {
        float rowW = wy[j];
        [unroll]
        for (int i = 0; i < 4; i++)
        {
            float2 sampleUV = (base + float2(i - 1, j - 1) + 0.5) * invTexSize;
            result += gSRVMap[gTerrainHeightMapIndex].SampleLevel(gSamplerLinearClamp, sampleUV, 0).r * wx[i] * rowW;
        }
    }
    return result;
}

// --- Terrain height sampling for domain shader ---

float SampleHeightLinear(float2 tileUV)
{
    return gSRVMap[gTerrainHeightMapIndex].SampleLevel(gSamplerLinearClamp, tileUV, 0).r;
}

float SampleTerrainHeight(float2 tileUV)
{
    return SampleHeightLinear(tileUV);
}

float3 SampleTerrainNormal(float2 tileUV)
{
    return gSRVMap[gTerrainNormalMapIndex].SampleLevel(gSamplerLinearClamp, tileUV, 0).xyz;
}

float4 SampleMaterialWeights(float2 tileUV)
{
    return gSRVMap[gTerrainWeightMapIndex].SampleLevel(gSamplerLinearClamp, tileUV, 0);
}

#endif // TERRAIN_COMMON_HLSLI
