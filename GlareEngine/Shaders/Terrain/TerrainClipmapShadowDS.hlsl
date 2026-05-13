#include "TerrainCommon.hlsli"

struct ShadowDomainOut
{
    float4 PosH : SV_POSITION;
};

[domain("quad")]
ShadowDomainOut main(
    ClipmapPatchTess patchTess,
    float2 uv : SV_DomainLocation,
    const OutputPatch<ClipmapHullOut, 4> quad)
{
    ShadowDomainOut dout;

    // Bilinear interpolation of tile UVs
    float2 tileUV = lerp(
        lerp(quad[0].TileUV, quad[1].TileUV, uv.x),
        lerp(quad[2].TileUV, quad[3].TileUV, uv.x),
        uv.y
    );

    // Skirt detection
    bool isLeft   = tileUV.x < 0.0;
    bool isRight  = tileUV.x > 1.0;
    bool isTop    = tileUV.y < 0.0;
    bool isBottom = tileUV.y > 1.0;

    bool leftSkirt   = isLeft   && (gTerrainSkirtEdgeFlags & 1u);
    bool rightSkirt  = isRight  && (gTerrainSkirtEdgeFlags & 2u);
    bool topSkirt    = isTop    && (gTerrainSkirtEdgeFlags & 4u);
    bool bottomSkirt = isBottom && (gTerrainSkirtEdgeFlags & 8u);
    bool anySkirt    = leftSkirt || rightSkirt || topSkirt || bottomSkirt;

    float2 effectiveUV = tileUV;
    if (anySkirt)
    {
        if (leftSkirt)   effectiveUV.x = 0.0;
        if (rightSkirt)  effectiveUV.x = 1.0;
        if (topSkirt)    effectiveUV.y = 0.0;
        if (bottomSkirt) effectiveUV.y = 1.0;
        if (!leftSkirt && !rightSkirt) effectiveUV.x = clamp(effectiveUV.x, 0.0, 1.0);
        if (!topSkirt && !bottomSkirt) effectiveUV.y = clamp(effectiveUV.y, 0.0, 1.0);
    }
    else
    {
        effectiveUV = clamp(tileUV, 0.0, 1.0);
    }

    // World XZ from effective UV + tile grid offset
    float cellSize = gTerrainCellSize;
    float2 tileOffsetWorld = float2(gTerrainTileOffsetX, gTerrainTileOffsetY) * cellSize;
    float2 worldXZ = float2(
        effectiveUV.x * TERRAIN_TILE_SIZE * cellSize + tileOffsetWorld.x,
        effectiveUV.y * TERRAIN_TILE_SIZE * cellSize + tileOffsetWorld.y
    );

    // Sample heightmap
    float2 clampedTileUV = clamp(tileUV, 0.0, 1.0);
    float2 hmUV = TileToHeightmapUV(clampedTileUV);
    float height = (SampleTerrainHeight(hmUV) - 0.5) * 2.0 * gTerrainHeightScale;

    // Push skirt vertices downward
    if (leftSkirt)   height -= gTerrainSkirtDepth;
    if (rightSkirt)  height -= gTerrainSkirtDepth;
    if (topSkirt)    height -= gTerrainSkirtDepth;
    if (bottomSkirt) height -= gTerrainSkirtDepth;

    float3 posW = float3(worldXZ.x, height, worldXZ.y);

    // Project to shadow clip space (ViewProj contains light VP)
    dout.PosH = mul(gTerrainViewProj, float4(posW, 1.0));

    return dout;
}
