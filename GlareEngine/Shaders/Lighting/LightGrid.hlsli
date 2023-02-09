//Forward Plus lighting helper
#include "../Misc/CommonResource.hlsli"

//Light counts sync with console code
#define MAX_LIGHTS 1024
#define TILE_SIZE (1 + MAX_LIGHTS)

struct TileLightData
{
    float3 PositionWS;
    float RadiusSq;

    float3 PositionVS;
    uint Pad01;

    float3 color;
    uint Type;

    float3 ConeDir;
    float2 ConeAngles; // x = 1.0f / (cos(coneInner) - cos(coneOuter)), y = cos(coneOuter)

    float4x4 ShadowTextureMatrix;
};

uint2 GetTilePos(float2 pos, float2 invTileDim)
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


