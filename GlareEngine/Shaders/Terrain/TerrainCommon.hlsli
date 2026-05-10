#ifndef TERRAIN_COMMON_HLSLI
#define TERRAIN_COMMON_HLSLI

#include "../Misc/CommonResource.hlsli"

#define TERRAIN_CLIPMAP_LEVELS  10
#define TERRAIN_TILE_SIZE       64
#define TERRAIN_HEIGHTMAP_SIZE  128
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
    float4  ShadowPosH  : POSITION1;
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
    int         gClipmapLevel;
    float       gTerrainCellSize;
    float       gTerrainHeightScale;
    float       gTerrainTexScale;
    float       gStochasticSharpness;
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
    int2        _PadDetail;
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
    float   gNoiseScale;
    uint    gNoiseSeed;
    uint    gNoiseOctaves;
    float   gNoiseLacunarity;
    float   gNoisePersistence;
    float   gNoiseWarpStrength;
    float   gNoiseWarpScale;
    float   gNoiseSnowHeight;
    float   gNoiseSnowTransition;
    float   gNoiseStoneSlope;
    float   gNoiseStoneTransition;
    int     gNoiseLODLevel;
    float   gNoisePad0;
    float   gNoisePad1;
    float   gNoisePad2;
};

// --- Stochastic sampling utility ---

float2x2 StochasticRotation(float2 worldPos)
{
    // random22 scrambles both axes — avoids directional streaks from random12's 1D projection
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

    // Decorrelated 2D hash — avoids streak artifacts from 1D-projected random12
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
    return (tileUV * (float)TERRAIN_TILE_SIZE + 0.5) / (float)TERRAIN_HEIGHTMAP_SIZE;
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

float2 SampleTerrainNormal(float2 tileUV)
{
    return gSRVMap[gTerrainNormalMapIndex].SampleLevel(gSamplerLinearClamp, tileUV, 0).rg;
}

float4 SampleMaterialWeights(float2 tileUV)
{
    return gSRVMap[gTerrainWeightMapIndex].SampleLevel(gSamplerLinearClamp, tileUV, 0);
}

#endif // TERRAIN_COMMON_HLSLI
