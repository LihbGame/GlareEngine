#include "LightGrid.hlsli"
#include "../Misc/CommonResource.hlsli"

#define THREAD_GROUD_X 4
#define THREAD_GROUD_Y 4
#define THREAD_GROUD_Z 4

#define GROUD_THREAD_TOTAL_NUM THREAD_GROUD_X * THREAD_GROUD_Y * THREAD_GROUD_Z

struct LightGrid
{
    float offset;
    float count;
};

struct Cluter
{
    float4 minPoint;
    float4 maxPoint;
};

StructuredBuffer<TileLightData> lightBuffer         : register(t0);

RWStructuredBuffer<Cluter> ClusterList              : register(u0);
RWStructuredBuffer<LightGrid> LightGridList         : register(u1);
RWStructuredBuffer<float> GlobalLightIndexList      : register(u2);
RWStructuredBuffer<uint> globalIndexCount           : register(u3);
RWStructuredBuffer<float> ClusterActiveList         : register(u4);

cbuffer CSConstant : register(b0)
{
    matrix View;
    matrix InvProj;
    float ScreenWidth;
    float ScreenHeight;
    float LightCount;
};

[numthreads(THREAD_GROUD_X, THREAD_GROUD_Y, THREAD_GROUD_Z)]
void main( uint3 DTid : SV_DispatchThreadID )
{
}