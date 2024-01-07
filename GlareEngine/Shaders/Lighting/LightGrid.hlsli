//Forward Plus lighting helper

//Light counts sync with console code
#define MAX_LIGHTS 1024
#define MAX_TILED_LIGHT 512
#define TILE_SIZE (1 + MAX_TILED_LIGHT)

struct LightGrid
{
    uint offset;
    float count;
};

struct Cluter
{
    float4 minPoint;
    float4 maxPoint;
};

struct TileLightData
{
    float3 PositionWS;
    float RadiusSq;

    float3 Color;
    uint Type;

    float3 PositionVS;
    uint ShadowConeIndex;

    float3 ConeDir;
    float2 ConeAngles; // x = 1.0f / (cos(coneInner) - cos(coneOuter)), y = cos(coneOuter)

    float4x4 ShadowTextureMatrix;
};

uint2 GetTilePos(uint2 pos, float2 invTileDim)
{
    return pos * invTileDim;
}
uint GetTileIndex(uint2 tilePos, uint tileCountX)
{
    return tilePos.y * tileCountX + tilePos.x;
}
uint GetTileOffset(uint tileIndex)
{
    return tileIndex * TILE_SIZE;
}


