#ifndef TERRAIN_COMMON_HLSLI
#define TERRAIN_COMMON_HLSLI

#include "../Misc/CommonResource.hlsli"

#define TERRAIN_CLIPMAP_LEVELS  10
#define TERRAIN_TILE_SIZE       64
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
    int         gTerrainLayerAlbedo[5];
    int         gTerrainLayerNormal[5];
    int         gTerrainLayerRoughness[5];
    int         gTerrainLayerMetallic[5];
    int         gTerrainLayerAO[5];
};

// --- Noise generation constant buffer ---

cbuffer TerrainNoiseCB : register(b0)
{
    float3  gNoiseCameraPos;
    float   gNoiseCellSize;
    int2    gNoiseTileOffset;
    int     gNoiseTileSize;
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
    float angle = random12(worldPos * 0.137) * PI2;
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

    float blend = random12(baseUV * 7.41 + worldPos * 0.03);
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

    float blend = random12(baseUV * 7.41 + worldPos * 0.03);
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

    float blend = random12(baseUV * 7.41 + worldPos * 0.03);
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
    // Reduce tessellation for coarser LOD levels
    tess /= float(1 << gClipmapLevel);
    tess = max(tess, gTerrainMinTess);
    return tess;
}

// --- Terrain height sampling for domain shader ---

float SampleTerrainHeight(float2 tileUV)
{
    return gSRVMap[gTerrainHeightMapIndex].SampleLevel(gSamplerLinearClamp, tileUV, 0).r;
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
