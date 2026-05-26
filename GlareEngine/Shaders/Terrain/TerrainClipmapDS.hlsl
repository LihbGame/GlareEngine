#include "TerrainCommon.hlsli"

[domain("quad")]
ClipmapDomainOut main(
    ClipmapPatchTess patchTess,
    float2 uv : SV_DomainLocation,
    const OutputPatch<ClipmapHullOut, 4> quad)
{
    ClipmapDomainOut dout;

    // Bilinear interpolation of tile UVs
    float2 tileUV = lerp(
        lerp(quad[0].TileUV, quad[1].TileUV, uv.x),
        lerp(quad[2].TileUV, quad[3].TileUV, uv.x),
        uv.y
    );

    // Detect which edge this vertex is on (border vertices have tileUV outside [0,1])
    bool isLeft   = tileUV.x < 0.0;
    bool isRight  = tileUV.x > 1.0;
    bool isTop    = tileUV.y < 0.0;
    bool isBottom = tileUV.y > 1.0;

    // Check if this vertex's edge has a skirt flag (LOD boundary).
    // Vertical skirt: XZ clamped to tile edge, only Y pushed down.
    // Non-skirt border: fully clamp to degenerate quad (avoid z-fighting).
    bool leftSkirt   = isLeft   && (gTerrainSkirtEdgeFlags & 1u);
    bool rightSkirt  = isRight  && (gTerrainSkirtEdgeFlags & 2u);
    bool topSkirt    = isTop    && (gTerrainSkirtEdgeFlags & 4u);
    bool bottomSkirt = isBottom && (gTerrainSkirtEdgeFlags & 8u);
    bool anySkirt    = leftSkirt || rightSkirt || bottomSkirt || topSkirt;

    float2 effectiveUV = tileUV;
    if (anySkirt)
    {
        // Clamp skirt axes to tile edge for vertical wall
        if (leftSkirt)   effectiveUV.x = 0.0;
        if (rightSkirt)  effectiveUV.x = 1.0;
        if (topSkirt)    effectiveUV.y = 0.0;
        if (bottomSkirt) effectiveUV.y = 1.0;

        // Clamp remaining out-of-bounds axes (corners with partial skirt)
        if (!leftSkirt && !rightSkirt) effectiveUV.x = clamp(effectiveUV.x, 0.0, 1.0);
        if (!topSkirt && !bottomSkirt) effectiveUV.y = clamp(effectiveUV.y, 0.0, 1.0);
    }
    else
    {
        effectiveUV = clamp(tileUV, 0.0, 1.0);
    }

    // Compute world XZ from effective UV + constant buffer tile grid offset
    float cellSize = gTerrainCellSize;
    float2 tileOffsetWorld = float2(gTerrainTileOffsetX, gTerrainTileOffsetY) * cellSize;
    float2 worldXZ = float2(
        effectiveUV.x * TERRAIN_TILE_SIZE * cellSize + tileOffsetWorld.x,
        effectiveUV.y * TERRAIN_TILE_SIZE * cellSize + tileOffsetWorld.y
    );

    // Convert tile UV to heightmap UV and clamp to [0,1] for sampling.
    float2 clampedTileUV = clamp(tileUV, 0.0, 1.0);
    float2 hmUV = TileToHeightmapUV(clampedTileUV);

    // Sample heightmap and denormalize from [0,1] to world-space height
    float height = (SampleTerrainHeight(hmUV) - 0.5) * 2.0 * gTerrainHeightScale;

    // Push skirt vertices downward to hide LOD seam gaps
    if (leftSkirt)   height -= gTerrainSkirtDepth;
    if (rightSkirt)  height -= gTerrainSkirtDepth;
    if (topSkirt)    height -= gTerrainSkirtDepth;
    if (bottomSkirt) height -= gTerrainSkirtDepth;

    // Construct world position
    dout.PosW = float3(worldXZ.x, height, worldXZ.y);
    dout.WorldXZ = worldXZ;

    // Sample the generated normal map as a full XYZ world-space normal.
    float3 normal = normalize(SampleTerrainNormal(hmUV));

    // Build tangent from world-space X axis, orthogonalized against normal.
    float3 tangent = normalize(float3(1.0, 0.0, 0.0) - normal.x * normal);

    dout.NormalW = normal;
    dout.TangentW = tangent;

    // Sample material weights
    dout.MatWeights = SampleMaterialWeights(hmUV);

    // Project to clip space
    dout.PosH = mul(gTerrainViewProj, float4(dout.PosW, 1.0));

    // Motion vectors: current and previous clip positions
    dout.CurPosition = dout.PosH.xyw;
    float4 prePosH = mul(gTerrainPreViewProj, float4(dout.PosW, 1.0));
    dout.PrePosition = prePosH.xyw;

    dout.TileUV = tileUV;

    return dout;
}
