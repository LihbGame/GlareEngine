#include "TerrainCommon.hlsli"

// Snap to nearest power of 2 (minimum 1) for cross-LOD edge alignment.
// LOD N's tess = 2^N * LOD 0's tess, guaranteeing coarse edge vertices
// align with fine edge vertices (T_coarse = 2 * T_fine).
float SnapToPow2(float x)
{
    return exp2(round(log2(max(x, 1.0))));
}

ClipmapPatchTess CalcClipmapPatchConstants(
    InputPatch<ClipmapVertexOut, 4> patch,
    uint patchID : SV_PrimitiveID)
{
    ClipmapPatchTess pt;

    // Compute world positions from tile UV + constant buffer tile offset
    float cellSize = gTerrainCellSize;
    float2 tileOffsetWorld = float2(gTerrainTileOffsetX, gTerrainTileOffsetY) * cellSize;

    float3 worldPos[4];
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        float2 uv = patch[i].TileUV;
        worldPos[i] = float3(
            (uv.x * TERRAIN_TILE_SIZE * cellSize) + tileOffsetWorld.x,
            0,
            (uv.y * TERRAIN_TILE_SIZE * cellSize) + tileOffsetWorld.y
        );
    }

    // Frustum culling using bounds
    float minY = patch[0].BoundsY.x;
    float maxY = patch[0].BoundsY.y;

    float3 vMin = float3(min(worldPos[0].x, worldPos[2].x), minY, min(worldPos[0].z, worldPos[2].z));
    float3 vMax = float3(max(worldPos[1].x, worldPos[3].x), maxY, max(worldPos[1].z, worldPos[3].z));
    float3 boxCenter = 0.5 * (vMin + vMax);
    float3 boxExtents = 0.5 * (vMax - vMin) + float3(cellSize, 0, cellSize);

    if (AABBOutsideFrustumTest(boxCenter, boxExtents, gTerrainFrustumPlanes))
    {
        pt.EdgeTess[0] = 0;
        pt.EdgeTess[1] = 0;
        pt.EdgeTess[2] = 0;
        pt.EdgeTess[3] = 0;
        pt.InsideTess[0] = 0;
        pt.InsideTess[1] = 0;
        return pt;
    }

    // Distance-based tessellation inversely scaled by LOD level.
    // LOD N divides raw tess by 2^N, so coarser levels get fewer subdivisions.
    // After pow2 snapping: T_coarse = T_fine / 2, guaranteeing aligned vertices
    // (every coarse vertex coincides with a fine vertex).
    float invLodScale = 1.0 / exp2((float)gClipmapLevel);

    float3 e0 = 0.5 * (worldPos[0] + worldPos[2]);
    float3 e1 = 0.5 * (worldPos[0] + worldPos[1]);
    float3 e2 = 0.5 * (worldPos[1] + worldPos[3]);
    float3 e3 = 0.5 * (worldPos[2] + worldPos[3]);
    float3 center = 0.25 * (worldPos[0] + worldPos[1] + worldPos[2] + worldPos[3]);

    pt.EdgeTess[0] = SnapToPow2(CalcClipmapTessFactor(e0) * invLodScale);
    pt.EdgeTess[1] = SnapToPow2(CalcClipmapTessFactor(e1) * invLodScale);
    pt.EdgeTess[2] = SnapToPow2(CalcClipmapTessFactor(e2) * invLodScale);
    pt.EdgeTess[3] = SnapToPow2(CalcClipmapTessFactor(e3) * invLodScale);

    float centerTess = SnapToPow2(CalcClipmapTessFactor(center) * invLodScale);
    pt.InsideTess[0] = centerTess;
    pt.InsideTess[1] = centerTess;

    return pt;
}

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(4)]
[patchconstantfunc("CalcClipmapPatchConstants")]
[maxtessfactor(64.0)]
ClipmapHullOut main(
    InputPatch<ClipmapVertexOut, 4> ip,
    uint i : SV_OutputControlPointID,
    uint patchId : SV_PrimitiveID)
{
    ClipmapHullOut hout;
    hout.PosW = ip[i].PosW;
    hout.TileUV = ip[i].TileUV;
    hout.LODLevel = ip[i].LODLevel;
    hout.TileOffset = ip[i].TileOffset;
    return hout;
}
