#include "LightGrid.hlsli"
#include "../Misc/CommonResource.hlsli"

#define THREAD_GROUD_X 4
#define THREAD_GROUD_Y 4
#define THREAD_GROUD_Z 4

#define GROUD_THREAD_TOTAL_NUM THREAD_GROUD_X * THREAD_GROUD_Y * THREAD_GROUD_Z
#define MAX_LIGHT_PER_CLUSTER 1024

StructuredBuffer<TileLightData> lightBuffer         : register(t0);
StructuredBuffer<Cluter> ClusterList                : register(t1);
StructuredBuffer<float> ClusterActiveList           : register(t2);

RWStructuredBuffer<LightGrid> LightGridList         : register(u0);
RWStructuredBuffer<float> GlobalLightIndexList      : register(u1);
RWStructuredBuffer<uint> GlobalIndexOffset          : register(u2);


cbuffer CSConstant : register(b0)
{
    float3 TileSizes;
    float LightCount;
};

[numthreads(THREAD_GROUD_X, THREAD_GROUD_Y, THREAD_GROUD_Z)]
void main(uint3 groupId : SV_GroupID,
        uint3 groupThreadId : SV_GroupThreadID,
        uint groupIndex : SV_GroupIndex,
        uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (all(dispatchThreadId < (uint3) TileSizes))
    {
        uint clusterIndex = dispatchThreadId.x + dispatchThreadId.y * (uint) TileSizes.x + dispatchThreadId.z * (uint) TileSizes.x * (uint) TileSizes.y;
    
    //Unused Cluster
        if (ClusterActiveList[clusterIndex] == 0.0)
        {
            LightGridList[clusterIndex].offset = GlobalIndexOffset[0];
            LightGridList[clusterIndex].count = 0.0;
            return;
        }
    
        uint visibleLightIndexs[MAX_LIGHT_PER_CLUSTER];
        uint visibleLightCount = 0;
    
        uint lightCountInt = (uint) LightCount;
    
        for (uint LightIndex = 0; LightIndex < lightCountInt; LightIndex += GROUD_THREAD_TOTAL_NUM)
        {
            TileLightData lightData = lightBuffer[LightIndex];
            float lightCullRadius = sqrt(lightData.RadiusSq);

            Sphere sphere = { lightData.PositionVS.xyz, lightCullRadius };
        
            AABB clusterAABB;
            clusterAABB.Center = (ClusterList[clusterIndex].minPoint + ClusterList[clusterIndex].maxPoint).xyz / 2.0f;
            clusterAABB.Extend = ClusterList[clusterIndex].maxPoint.xyz - clusterAABB.Center;
        
            if (LightIndex < lightCountInt && LightIndex < MAX_LIGHT_PER_CLUSTER && IntersectionAABBvsSphere(sphere, clusterAABB))
            {
                visibleLightIndexs[visibleLightCount] = LightIndex;
                visibleLightCount++;
            }
        }
    
    //We want all thread groups to have completed the light tests before continuing
        GroupMemoryBarrierWithGroupSync();
    
        uint offset;
        InterlockedAdd(GlobalIndexOffset[0], visibleLightCount, offset);

        for (uint i = 0; i < visibleLightCount; ++i)
        {
            GlobalLightIndexList[offset + i] = visibleLightIndexs[i];
        }

        LightGridList[clusterIndex].offset = (float) offset;
        LightGridList[clusterIndex].count = (float) visibleLightCount;
    }
}