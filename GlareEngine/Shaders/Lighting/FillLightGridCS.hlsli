#include "LightGrid.hlsli"

cbuffer CSConstants : register(b0)
{
    uint ViewportWidth;
    uint ViewportHeight;
    float InvTileDim;
    float RcpZMagic;
    uint TileCountX;
    float4x4 ViewProjMatrix;
};


StructuredBuffer<LightData> lightBuffer : register(t0);
Texture2D<float> depthTex : register(t1);
RWStructuredBuffer<uint> lightGrid : register(u0);


groupshared uint minDepthUInt;
groupshared uint maxDepthUInt;

groupshared uint tileLightCountSphere;
groupshared uint tileLightCountCone;
groupshared uint tileLightCountConeShadowed;

groupshared uint tileLightIndicesSphere[MAX_LIGHTS];
groupshared uint tileLightIndicesCone[MAX_LIGHTS];
groupshared uint tileLightIndicesConeShadowed[MAX_LIGHTS];


[numthreads(8, 8, 1)]
void main(
    uint2 Gid : SV_GroupID,
    uint2 GTid : SV_GroupThreadID,
    uint GI : SV_GroupIndex)
{
    // initialize shared data
    if (GI == 0)
    {
        tileLightCountSphere = 0;
        tileLightCountCone = 0;
        tileLightCountConeShadowed = 0;
        minDepthUInt = 0xffffffff;
        maxDepthUInt = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // Read all depth values for this tile and compute the tile min and max values
    for (uint dx = GTid.x; dx < WORK_GROUP_SIZE_X; dx += 8)
    {
        for (uint dy = GTid.y; dy < WORK_GROUP_SIZE_Y; dy += 8)
        {
            uint2 DTid = Gid * uint2(WORK_GROUP_SIZE_X, WORK_GROUP_SIZE_Y) + uint2(dx, dy);

            // If pixel coordinates are in bounds...
            if (DTid.x < ViewportWidth && DTid.y < ViewportHeight)
            {
                // Load and compare depth
                uint depthUInt = asuint(depthTex[DTid.xy]);
                InterlockedMin(minDepthUInt, depthUInt);
                InterlockedMax(maxDepthUInt, depthUInt);
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();

}