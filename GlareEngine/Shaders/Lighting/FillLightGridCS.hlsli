#include "LightGrid.hlsli"
#include "../Misc/CommonResource.hlsli"

#define SM_6_0
//#define AABBCULLING
#define ADVANCED_CULLING ///Harada Siggraph 2012 2.5D culling

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


StructuredBuffer<TileLightData>     lightBuffer     : register(t0);
Texture2D<float>                    depthTex        : register(t1);
RWStructuredBuffer<uint>            lightGrid       : register(u0);


groupshared uint minDepthUInt;
groupshared uint maxDepthUInt;
groupshared uint DepthMask;


#ifdef AABBCULLING
groupshared AABB GroupAABB;			// frustum AABB around min-max depth in View Space
#endif

groupshared uint tileLightCountSphere;
groupshared uint tileLightCountCone;
groupshared uint tileLightCountConeShadowed;

groupshared uint tileLightIndicesSphere[MAX_LIGHTS];
groupshared uint tileLightIndicesCone[MAX_LIGHTS];
groupshared uint tileLightIndicesConeShadowed[MAX_LIGHTS];

// Now build the frustum planes from the view space points
groupshared Frustum frustum;


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

inline uint ConstructEntityMask(in float depthRangeMin, in float depthRangeRecip, in Sphere bounds)
{
    // We create a entity mask to decide if the entity is really touching something
    // If we do an OR operation with the depth slices mask, we instantly get if the entity is contributing or not
    // we do this in view space

    const float fMin = bounds.c.z - bounds.r;
    const float fMax = bounds.c.z + bounds.r;
    const uint __entitymaskcellindexSTART = max(0, min(31, floor((fMin - depthRangeMin) * depthRangeRecip)));
    const uint __entitymaskcellindexEND = max(0, min(31, floor((fMax - depthRangeMin) * depthRangeRecip)));

    //// Unoptimized mask construction with loop:
    //// Construct mask from START to END:
    ////          END         START
    ////	0000000000111111111110000000000
    //uint uLightMask = 0;
    //for (uint c = __entitymaskcellindexSTART; c <= __entitymaskcellindexEND; ++c)
    //{
    //	uLightMask |= 1u << c;
    //}


    // Optimized mask construction, without loop:
    //	- First, fill full mask:
    //	1111111111111111111111111111111
    uint uLightMask = 0xFFFFFFFF;
    //	- Then Shift right with spare amount to keep mask only:
    //	0000000000000000000011111111111
    uLightMask >>= 31u - (__entitymaskcellindexEND - __entitymaskcellindexSTART);
    //	- Last, shift left with START amount to correct mask position:
    //	0000000000111111111110000000000
    uLightMask <<= __entitymaskcellindexSTART;

    return uLightMask;
}




[numthreads(8, 8, 1)]
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


    float minDepth = FLT_MAX;
    float maxDepth = 0;
    // Read all depth values for this tile and compute the tile min and max values
    for (uint dx = GTid.x; dx < WORK_GROUP_SIZE_X; dx += 8)
    {
        for (uint dy = GTid.y; dy < WORK_GROUP_SIZE_Y; dy += 8)
        {
            uint2 DTid = Gid * uint2(WORK_GROUP_SIZE_X, WORK_GROUP_SIZE_Y) + uint2(dx, dy);
            DTid = min(DTid, uint2(ViewportWidth-1, ViewportHeight-1));// avoid loading from outside the texture, it messes up the min-max depth!
            // Load and compare depth
            float depthfloat = depthTex[DTid.xy];
            minDepth = min(minDepth, depthfloat);
            maxDepth = max(maxDepth, depthfloat);
        }
    }
#ifdef SM_6_0
    float wave_local_min = WaveActiveMin(minDepth);
    float wave_local_max = WaveActiveMax(maxDepth);

    if (WaveIsFirstLane())
    {
        InterlockedMin(minDepthUInt, asuint(wave_local_min));
        InterlockedMax(maxDepthUInt, asuint(wave_local_max));
    }
#else
    InterlockedMin(minDepthUInt, asuint(minDepth));
    InterlockedMax(maxDepthUInt, asuint(maxDepth));
#endif

    GroupMemoryBarrierWithGroupSync();

    float fMinDepth = asfloat(minDepthUInt);
    float fMaxDepth = asfloat(maxDepthUInt);

    // Convert depth values to view space.
    float maxDepthVS = ScreenToView(float4(0, 0, fMinDepth, 1), float2(ViewportWidth, ViewportHeight), InverseProjection).z;
    float minDepthVS = ScreenToView(float4(0, 0, fMaxDepth, 1), float2(ViewportWidth, ViewportHeight), InverseProjection).z;

    if (GI == 0)
    {
#ifdef AABBCULLING
        float4 viewSpace[8];
        uint scale = WORK_GROUP_SIZE_Y / 8;
        uint2 DispatchThreadID = DTI.xy * scale;
        // Top left point -far
        viewSpace[0] = float4(DispatchThreadID.xy, fMinDepth, 1.0f);
        // Top right point -far
        viewSpace[1] = float4(float2(DispatchThreadID.x + WORK_GROUP_SIZE_Y, DispatchThreadID.y), fMinDepth, 1.0f);
        // Bottom left point -far
        viewSpace[2] = float4(float2(DispatchThreadID.x, DispatchThreadID.y + WORK_GROUP_SIZE_Y), fMinDepth, 1.0f);
        // Bottom right point -far
        viewSpace[3] = float4(float2(DispatchThreadID.x + WORK_GROUP_SIZE_Y, DispatchThreadID.y + WORK_GROUP_SIZE_Y), fMinDepth, 1.0f);

        // Top left point -near
        viewSpace[4] = float4(DispatchThreadID.xy, fMaxDepth, 1.0f);
        // Top right point -near
        viewSpace[5] = float4(float2(DispatchThreadID.x + WORK_GROUP_SIZE_Y, DispatchThreadID.y), fMaxDepth, 1.0f);
        // Bottom left point -near
        viewSpace[6] = float4(float2(DispatchThreadID.x, DispatchThreadID.y + WORK_GROUP_SIZE_Y), fMaxDepth, 1.0f);
        // Bottom right point -near
        viewSpace[7] = float4(float2(DispatchThreadID.x + WORK_GROUP_SIZE_Y, DispatchThreadID.y + WORK_GROUP_SIZE_Y), fMaxDepth, 1.0f);

        // Now convert the screen space points to view space
        uint i = 0;
        [unroll]
        for (i = 0; i < 8; i++)
        {
            viewSpace[i].xyz = ScreenToView(viewSpace[i], float2(ViewportWidth, ViewportHeight), InverseProjection).xyz;
        }

        float3 minAABB = 10000000;
        float3 maxAABB = -10000000;
        [unroll]
        for (i = 0; i < 8; ++i)
        {
            minAABB = min(minAABB, viewSpace[i].xyz);
            maxAABB = max(maxAABB, viewSpace[i].xyz);
        }

        GroupAABB.Center = (minAABB + maxAABB) / 2.0f;
        GroupAABB.Extend = maxAABB - GroupAABB.Center;



#else
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
        [unroll]
        for (int i = 0; i < 4; i++)
        {
            viewSpace[i] = ScreenToView(screenSpace[i], float2(ViewportWidth, ViewportHeight), InverseProjection).xyz;
        }

        // Left plane
        frustum.planes[0] = ComputePlane(eyePos, viewSpace[2], viewSpace[0]);
        // Right plane
        frustum.planes[1] = ComputePlane(eyePos, viewSpace[1], viewSpace[3]);
        // Top plane
        frustum.planes[2] = ComputePlane(eyePos, viewSpace[0], viewSpace[1]);
        // Bottom plane
        frustum.planes[3] = ComputePlane(eyePos, viewSpace[3], viewSpace[2]);
#endif
    }

#ifdef ADVANCED_CULLING
    // We divide the minmax depth bounds to 32 equal slices
    // then we mark the occupied depth slices with atomic or from each thread
    // we do all this in linear (view) space
    const float __depthRangeRecip = 31.0f / (maxDepthVS - minDepthVS);
    uint __depthmaskUnrolled = 0;

    for (uint DX = GTid.x; DX < WORK_GROUP_SIZE_X; DX += 8)
    {
        for (uint DY = GTid.y; DY < WORK_GROUP_SIZE_Y; DY += 8)
        {
            uint2 DTid = Gid * uint2(WORK_GROUP_SIZE_X, WORK_GROUP_SIZE_Y) + uint2(DX, DY);
            DTid = min(DTid, uint2(ViewportWidth - 1, ViewportHeight - 1));// avoid loading from outside the texture, it messes up the min-max depth!
            float realDepthVS = ScreenToView(float4(0, 0, depthTex[DTid.xy], 1), float2(ViewportWidth, ViewportHeight), InverseProjection).z;
            const uint __depthmaskcellindex = max(0, min(31, floor((realDepthVS - minDepthVS) * __depthRangeRecip)));
            __depthmaskUnrolled |= 1u << __depthmaskcellindex;
        }
    }

#ifdef SM_6_0
    uint wave_depth_mask = WaveActiveBitOr(__depthmaskUnrolled);
    if (WaveIsFirstLane())
    {
        InterlockedOr(DepthMask, wave_depth_mask);
    }
#else
    InterlockedOr(DepthMask, __depthmaskUnrolled);
#endif

#endif

    GroupMemoryBarrierWithGroupSync();

    const uint depth_mask = DepthMask; // take out from groupshared into register


    uint tileIndex = GetTileIndex(Gid.xy, TileCountX);
    uint tileOffset = GetTileOffset(tileIndex);

    // Find set of lights that overlap this tile
    for (uint lightIndex = GI; lightIndex < MAX_LIGHTS; lightIndex += 64)
    {
        TileLightData lightData = lightBuffer[lightIndex];
        float lightCullRadius = sqrt(lightData.RadiusSq);

        Sphere sphere = { lightData.PositionVS.xyz, lightCullRadius };

#ifdef ADVANCED_CULLING
        if ((depth_mask & ConstructEntityMask(minDepthVS, __depthRangeRecip, sphere)) == 0)
            continue;
#endif


#ifdef AABBCULLING
        if (!IntersectionAABBvsSphere(sphere, GroupAABB))
            continue;
#else
        if (!SphereInsideFrustum(sphere, frustum, minDepthVS, maxDepthVS))
            continue;
#endif


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
            lightGrid[storeOffset] = tileLightIndicesSphere[n];
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