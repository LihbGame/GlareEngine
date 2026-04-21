#include "TerrainCommon.hlsli"

ClipmapVertexOut main(ClipmapVertexIn vin, uint instanceID : SV_InstanceID)
{
    ClipmapVertexOut vout;

    // Tile-local grid position -> UV [0, 1]
    vout.TileUV = vin.Tex;

    // Pass position for tessellation (will be expanded in HS/DS)
    vout.PosW = vin.Position;

    // Bounds for frustum culling
    vout.BoundsY = vin.BoundsY;

    // LOD and tile offset are read from constant buffer in HS/DS
    vout.LODLevel = gClipmapLevel;
    vout.TileOffset = int2(gTerrainTileOffsetX, gTerrainTileOffsetY);

    return vout;
}
