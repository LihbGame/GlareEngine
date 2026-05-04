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

    // Compute world XZ from tile UV + constant buffer tile grid offset
    float cellSize = gTerrainCellSize;
    float2 tileOffsetWorld = float2(gTerrainTileOffsetX, gTerrainTileOffsetY) * cellSize;
    float2 worldXZ = float2(
        tileUV.x * TERRAIN_TILE_SIZE * cellSize + tileOffsetWorld.x,
        tileUV.y * TERRAIN_TILE_SIZE * cellSize + tileOffsetWorld.y
    );

    // Sample heightmap and denormalize from [0,1] to world-space height
    float height = (SampleTerrainHeight(tileUV) - 0.5) * 2.0 * gTerrainHeightScale;

    // Construct world position
    dout.PosW = float3(worldXZ.x, height, worldXZ.y);
    dout.WorldXZ = worldXZ;

    // Sample packed normal and reconstruct
    float2 packedNormal = SampleTerrainNormal(tileUV);
    float3 normal = normalize(float3(packedNormal.x, 1.0, packedNormal.y));

    // Build tangent from world-space X axis, orthogonalized against normal
    float3 tangent = normalize(float3(1.0, 0.0, 0.0) - normal.x * normal);
    float3 bitangent = cross(normal, tangent);

    dout.NormalW = normal;
    dout.TangentW = tangent;

    // Sample material weights
    dout.MatWeights = SampleMaterialWeights(tileUV);

    // Shadow coordinates
    dout.ShadowPosH = mul(float4(dout.PosW, 1.0), gShadowTransform);

    // Project to clip space
    dout.PosH = mul(gTerrainViewProj,float4(dout.PosW, 1.0));

    dout.TileUV = tileUV;

    return dout;
}
