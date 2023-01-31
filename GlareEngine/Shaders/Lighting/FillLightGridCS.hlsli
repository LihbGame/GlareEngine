#include "LightGrid.hlsli"

cbuffer CSConstants : register(b0)
{
    uint ViewportWidth;
    uint ViewportHeight;
    float InvTileDim;
    float RcpZMagic;
    uint TileCountX;
    float4x4 ViewProjMatrix;
    float4x4 InverseProjection;
};


StructuredBuffer<TileLightData> lightBuffer : register(t0);
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


// Convert clip space coordinates to view space
float4 ClipToView(float4 clip)
{
    // View space position.
    float4 view = mul(InverseProjection, clip);
    // Perspecitive projection.
    view = view / view.w;

    return view;
}

// Convert screen space coordinates to view space.
float4 ScreenToView(float4 screen)
{
    // Convert to normalized texture coordinates
    float2 texCoord = screen.xy / float2(ViewportWidth, ViewportHeight);

    // Convert to clip space
    float4 clip = float4(float2(texCoord.x, 1.0f - texCoord.y) * 2.0f - 1.0f, screen.z, screen.w);

    return ClipToView(clip);
}

// Compute a plane from 3 noncollinear points that form a triangle.
// This equation assumes a right-handed (counter-clockwise winding order) 
// coordinate system to determine the direction of the plane normal.
Plane ComputePlane(float3 p0, float3 p1, float3 p2)
{
    Plane plane;

    float3 v0 = p1 - p0;
    float3 v2 = p2 - p0;

    plane.N = normalize(cross(v0, v2));

    // Compute the distance to the origin using p0.
    plane.d = dot(plane.N, p0);

    return plane;
}

// Check to see if a sphere is fully behind (inside the negative halfspace of) a plane.
// Source: Real-time collision detection, Christer Ericson (2005)
bool SphereInsidePlane(Sphere sphere, Plane plane)
{
    return dot(plane.N, sphere.c) - plane.d < -sphere.r;
}



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


    //Frustum Calculate 

    float fMinDepth = asfloat(minDepthUInt);
    float fMaxDepth = asfloat(maxDepthUInt);

    // Convert depth values to view space.
    float minDepthVS = ScreenToView(float4(0, 0, fMinDepth, 1)).z;
    float maxDepthVS = ScreenToView(float4(0, 0, fMaxDepth, 1)).z;



    uint tileIndex = GetTileIndex(Gid.xy, TileCountX);
    uint tileOffset = GetTileOffset(tileIndex);

    // Find set of lights that overlap this tile
    for (uint lightIndex = GI; lightIndex < MAX_LIGHTS; lightIndex += 64)
    {
        LightData lightData = lightBuffer[lightIndex];
        float3 lightWorldPos = lightData.pos;
        float lightCullRadius = sqrt(lightData.radiusSq);

        bool overlapping = true;
        for (int p = 0; p < 6; p++)
        {
            float d = dot(lightWorldPos, frustumPlanes[p].xyz) + frustumPlanes[p].w;
            if (d < -lightCullRadius)
            {
                overlapping = false;
            }
        }

        if (!overlapping)
            continue;

        uint slot;

        switch (lightData.type)
        {
        case 0: // sphere
            InterlockedAdd(tileLightCountSphere, 1, slot);
            tileLightIndicesSphere[slot] = lightIndex;
            break;

        case 1: // cone
            InterlockedAdd(tileLightCountCone, 1, slot);
            tileLightIndicesCone[slot] = lightIndex;
            break;

        case 2: // cone w/ shadow map
            InterlockedAdd(tileLightCountConeShadowed, 1, slot);
            tileLightIndicesConeShadowed[slot] = lightIndex;
            break;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (GI == 0)
    {
        uint lightCount = tileLightCountSphere + tileLightCountCone + tileLightCountConeShadowed;

        lightGrid.Store(tileOffset, lightCount);

        uint storeOffset = tileOffset + 1;
        uint n;
        for (n = 0; n < tileLightCountSphere; n++)
        {
            lightGrid.Store(storeOffset, tileLightIndicesSphere[n]);
            storeOffset += 1;
        }
        for (n = 0; n < tileLightCountCone; n++)
        {
            lightGrid.Store(storeOffset, tileLightIndicesCone[n]);
            storeOffset += 1;
        }
        for (n = 0; n < tileLightCountConeShadowed; n++)
        {
            lightGrid.Store(storeOffset, tileLightIndicesConeShadowed[n]);
            storeOffset += 1;
        }
    }
}