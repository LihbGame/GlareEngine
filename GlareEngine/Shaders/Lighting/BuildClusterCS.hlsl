#include "../Misc/CommonResource.hlsli"

struct Cluter
{
    float4 minPoint;
    float4 maxPoint;
};

RWStructuredBuffer<Cluter> ClusterList : register(u0);

cbuffer ClusterConst : register(b0)
{
    matrix View;
    matrix InvProj;
    float3 tileSizes;
    float nearPlane;
    float2 perTileSize;
    float ScreenWidth;
    float ScreenHeight;
    float farPlane;
};

//Solving similar triangles
float3 LineIntersectionToZPlane(float3 a, float3 b, float z)
{
    float3 normal = float3(0.0, 0.0, 1.0);
    float3 ab = b - a;
    float t = (z - dot(normal, a)) / dot(normal, ab);
    float3 result = a + t * ab;
    return result;
}

[numthreads(1, 1, 1)]
void main(uint3 groupId : SV_GroupID,
	uint3 groupThreadId : SV_GroupThreadID,
	uint groupIndex : SV_GroupIndex,
	uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const float3 eyePos = float3(0.0, 0.0, 0.0);
    uint tileIndex = groupId.x + groupId.y * (uint) tileSizes.x + groupId.z * (uint) tileSizes.x * (uint) tileSizes.y;

	//Calculate the min and max point in screen, far plane, near plane exit error(forever zero)
    float4 maxPointSs = float4(float2(groupId.x + 1, groupId.y + 1) * perTileSize, 1.0, 1.0);
    float4 minPointSs = float4(groupId.xy * perTileSize, 1.0, 1.0);

	//MinPoint and MaxPoint of the cluster in view space(nearest plane, ndc pos.w = 0.0)
    float3 maxPointVs = ScreenToView(maxPointSs, float2(ScreenWidth, ScreenHeight), InvProj).xyz;
    float3 minPointVs = ScreenToView(minPointSs, float2(ScreenWidth, ScreenHeight), InvProj).xyz;

	//Near and far values of the cluster in view space, the split cluster method from siggraph 2016 idtech6
    float tileNear = nearPlane * pow(farPlane / nearPlane, groupId.z / tileSizes.z);
    float tileFar = nearPlane * pow(farPlane / nearPlane, (groupId.z + 1) / tileSizes.z);
      
	//find cluster min/max 4 point in view space
    float3 minPointNear = LineIntersectionToZPlane(eyePos, minPointVs, tileNear);
    float3 minPointFar = LineIntersectionToZPlane(eyePos, minPointVs, tileFar);
    float3 maxPointNear = LineIntersectionToZPlane(eyePos, maxPointVs, tileNear);
    float3 maxPointFar = LineIntersectionToZPlane(eyePos, maxPointVs, tileFar);

    float3 minPointAABB = min(min(minPointNear, minPointFar), min(maxPointNear, maxPointFar));
    float3 maxPointAABB = max(max(minPointNear, minPointFar), max(maxPointNear, maxPointFar));
	
    ClusterList[tileIndex].minPoint = float4(minPointAABB, 1.0);
    ClusterList[tileIndex].maxPoint = float4(maxPointAABB, 1.0);
}