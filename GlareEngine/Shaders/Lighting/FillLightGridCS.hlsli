#include "LightGrid.hlsli"
#include "../Misc/CommonResource.hlsli"
cbuffer CSConstants : register(b0)
{
    uint ViewportWidth;
    uint ViewportHeight;
    float InvTileDim;
    float RcpZMagic;
    float3 EyePositionWS;
    float4x4 InverseViewProj;
    float4x4 InverseProjection;
    uint TileCountX;
};


StructuredBuffer<TileLightData> lightBuffer : register(t0);
Texture2D<float> depthTex : register(t1);
RWStructuredBuffer<uint> lightGrid : register(u0);


groupshared uint minDepthUInt;
groupshared uint maxDepthUInt;
groupshared float depthFloatMin[64];
groupshared float depthFloatMax[64];

groupshared uint tileLightCountSphere;
groupshared uint tileLightCountCone;
groupshared uint tileLightCountConeShadowed;

groupshared uint tileLightIndicesSphere[MAX_LIGHTS];
groupshared uint tileLightIndicesCone[MAX_LIGHTS];
groupshared uint tileLightIndicesConeShadowed[MAX_LIGHTS];

// Now build the frustum planes from the view space points
groupshared Frustum frustum;

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
    return dot(plane.N, sphere.c) - plane.d > sphere.r;
}


// Check to see of a light is partially contained within the frustum.
bool SphereInsideFrustum(Sphere sphere, Frustum frustum, float zNear, float zFar)
{
    bool result = true;

    if (sphere.c.z + sphere.r < zNear || sphere.c.z - sphere.r > zFar)
    {
        result = false;
    }

    // Then check frustum planes
    for (int i = 0; i < 4 && result; i++)
    {
        if (SphereInsidePlane(sphere, frustum.planes[i]))
        {
            result = false;
        }
    }

    return result;
}


[numThreads(8, 8, 1)]
void main(
    uint2 Gid : SV_GroupID,
    uint2 GTid : SV_GroupThreadID,
    uint GI : SV_GroupIndex,
    uint2 DTI : SV_DispatchThreadID)
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


    float minDepth = 0;
    float maxDepth = 0xffffffff;
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
                float depthfloat = depthTex[DTid.xy];
                minDepth = min(minDepth, depthfloat);
                maxDepth = max(maxDepth, depthfloat);
            }
        }
    }

    depthFloatMin[GI] = minDepth;
    depthFloatMax[GI] = maxDepth;

    GroupMemoryBarrierWithGroupSync();

    InterlockedMin(minDepthUInt, asuint(depthFloatMin[GI]));
    InterlockedMax(maxDepthUInt, asuint(depthFloatMax[GI]));

    GroupMemoryBarrierWithGroupSync();

    float fMinDepth = asfloat(minDepthUInt);
    float fMaxDepth = asfloat(maxDepthUInt);

    // Convert depth values to view space.
    float maxDepthVS = ScreenToView(float4(0, 0, fMinDepth, 1)).z;
    float minDepthVS = ScreenToView(float4(0, 0, fMaxDepth, 1)).z;

    if (GI == 0)
    {
        //Frustum Calculate 

        // View space eye position is always at the origin.
        const float3 eyePos = float3(0, 0, 0);

        // Compute 4 points on the far clipping plane to use as the frustum vertices.
        float4 screenSpace[4];
        uint scale = WORK_GROUP_SIZE_Y / 8;
        uint2 DispatchThreadID = DTI.xy * scale;

        // Top left point
        screenSpace[0] = float4(DispatchThreadID.xy, 0.0f, 1.0f);
        // Top right point
        screenSpace[1] = float4(float2(DispatchThreadID.x + WORK_GROUP_SIZE_Y, DispatchThreadID.y), 0.0f, 1.0f);
        // Bottom left point
        screenSpace[2] = float4(float2(DispatchThreadID.x, DispatchThreadID.y + WORK_GROUP_SIZE_Y), 0.0f, 1.0f);
        // Bottom right point
        screenSpace[3] = float4(float2(DispatchThreadID.x + WORK_GROUP_SIZE_Y, DispatchThreadID.y + WORK_GROUP_SIZE_Y), 0.0f, 1.0f);

        float3 viewSpace[4];
        // Now convert the screen space points to view space
        for (int i = 0; i < 4; i++)
        {
            viewSpace[i] = ScreenToView(screenSpace[i]).xyz;
        }

        // Left plane
        frustum.planes[0] = ComputePlane(eyePos, viewSpace[2], viewSpace[0]);
        // Right plane
        frustum.planes[1] = ComputePlane(eyePos, viewSpace[1], viewSpace[3]);
        // Top plane
        frustum.planes[2] = ComputePlane(eyePos, viewSpace[0], viewSpace[1]);
        // Bottom plane
        frustum.planes[3] = ComputePlane(eyePos, viewSpace[3], viewSpace[2]);
    }

    GroupMemoryBarrierWithGroupSync();

    uint tileIndex = GetTileIndex(Gid.xy, TileCountX);
    uint tileOffset = GetTileOffset(tileIndex);

    // Find set of lights that overlap this tile
    for (uint lightIndex = GI; lightIndex < MAX_LIGHTS; lightIndex += 64)
    {
        TileLightData lightData = lightBuffer[lightIndex];
        float lightCullRadius = sqrt(lightData.RadiusSq);

        Sphere sphere = { lightData.PositionVS.xyz, lightCullRadius };
        if (!SphereInsideFrustum(sphere, frustum, minDepthVS, maxDepthVS))
            continue;

        uint slot;

        switch (lightData.Type)
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

        lightGrid[tileOffset] = lightCount;

        uint storeOffset = tileOffset + 1;
        uint n;
        for (n = 0; n < tileLightCountSphere; n++)
        {
            lightGrid[storeOffset]=tileLightIndicesSphere[n];
            storeOffset += 1;
        }
        for (n = 0; n < tileLightCountCone; n++)
        {
            lightGrid[storeOffset] = tileLightIndicesCone[n];
            storeOffset += 1;
        }
        for (n = 0; n < tileLightCountConeShadowed; n++)
        {
            lightGrid[storeOffset] = tileLightIndicesConeShadowed[n];
            storeOffset += 1;
        }
    }
}