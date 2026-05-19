#include "TerrainCommon.hlsli"

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

    // Distance-based tessellation inversely scaled by LOD level.
    // Fractional_even partitioning provides smooth interpolation between
    // tessellation levels, eliminating discrete LOD popping artifacts.
    float invLodScale = 1.0 / exp2((float)gClipmapLevel);

    float3 e0 = 0.5 * (worldPos[0] + worldPos[2]);
    float3 e1 = 0.5 * (worldPos[0] + worldPos[1]);
    float3 e2 = 0.5 * (worldPos[1] + worldPos[3]);
    float3 e3 = 0.5 * (worldPos[2] + worldPos[3]);
    float3 center = 0.25 * (worldPos[0] + worldPos[1] + worldPos[2] + worldPos[3]);

    pt.EdgeTess[0] = max(CalcClipmapTessFactor(e0) * invLodScale, 2.0);
    pt.EdgeTess[1] = max(CalcClipmapTessFactor(e1) * invLodScale, 2.0);
    pt.EdgeTess[2] = max(CalcClipmapTessFactor(e2) * invLodScale, 2.0);
    pt.EdgeTess[3] = max(CalcClipmapTessFactor(e3) * invLodScale, 2.0);

    float centerTess = max(CalcClipmapTessFactor(center) * invLodScale, 2.0);
    pt.InsideTess[0] = centerTess;
    pt.InsideTess[1] = centerTess;

    return pt;
}

[domain("quad")]
[partitioning("fractional_even")]
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
