#include "TerrainCommon.hlsli"

bool IsPatchCoveredByFinerLevel(float3 worldPos[4])
{
    if (gHasFinerLevel == 0)
    {
        return false;
    }

    float minX = min(min(worldPos[0].x, worldPos[1].x), min(worldPos[2].x, worldPos[3].x));
    float maxX = max(max(worldPos[0].x, worldPos[1].x), max(worldPos[2].x, worldPos[3].x));
    float minZ = min(min(worldPos[0].z, worldPos[1].z), min(worldPos[2].z, worldPos[3].z));
    float maxZ = max(max(worldPos[0].z, worldPos[1].z), max(worldPos[2].z, worldPos[3].z));

    return minX >= gFinerLevelMinX && maxX <= gFinerLevelMaxX &&
           minZ >= gFinerLevelMinZ && maxZ <= gFinerLevelMaxZ;
}

ClipmapPatchTess MakeCulledPatchTess()
{
    ClipmapPatchTess pt;
    pt.EdgeTess[0] = 0.0;
    pt.EdgeTess[1] = 0.0;
    pt.EdgeTess[2] = 0.0;
    pt.EdgeTess[3] = 0.0;
    pt.InsideTess[0] = 0.0;
    pt.InsideTess[1] = 0.0;
    return pt;
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

    if (IsPatchCoveredByFinerLevel(worldPos))
    {
        return MakeCulledPatchTess();
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

    float globalTessScale = max(gTerrainTessScale, 0.01);
    float minScaledTess = max(gTerrainMinTess * globalTessScale, 1.0);
    pt.EdgeTess[0] = clamp(CalcClipmapTessFactor(e0) * invLodScale * globalTessScale, minScaledTess, 64.0);
    pt.EdgeTess[1] = clamp(CalcClipmapTessFactor(e1) * invLodScale * globalTessScale, minScaledTess, 64.0);
    pt.EdgeTess[2] = clamp(CalcClipmapTessFactor(e2) * invLodScale * globalTessScale, minScaledTess, 64.0);
    pt.EdgeTess[3] = clamp(CalcClipmapTessFactor(e3) * invLodScale * globalTessScale, minScaledTess, 64.0);

    float centerTess = clamp(CalcClipmapTessFactor(center) * invLodScale * globalTessScale, minScaledTess, 64.0);
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
