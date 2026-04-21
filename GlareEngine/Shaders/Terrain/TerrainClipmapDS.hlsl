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

    // Sample heightmap for displacement
    float height = SampleTerrainHeight(tileUV);

    // Construct world position
    dout.PosW = float3(worldXZ.x, height, worldXZ.y);
    dout.WorldXZ = worldXZ;

    // Sample packed normal and reconstruct
    float2 packedNormal = SampleTerrainNormal(tileUV);
    float3 normal = normalize(float3(packedNormal.x, 1.0, packedNormal.y));

    // Build tangent frame from normal
    float3 up = abs(normal.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);

    dout.NormalW = normal;
    dout.TangentW = tangent;

    // Sample material weights
    dout.MatWeights = SampleMaterialWeights(tileUV);

    // Shadow coordinates
    dout.ShadowPosH = mul(float4(dout.PosW, 1.0), gShadowTransform);

    // Project to clip space
    dout.PosH = mul(float4(dout.PosW, 1.0), gTerrainViewProj);

    dout.TileUV = tileUV;

    return dout;
}
