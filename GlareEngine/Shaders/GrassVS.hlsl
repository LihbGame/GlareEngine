#include "TerrainConstBuffer.hlsli"


struct VertexIn
{
	float3 PosL    : POSITION;
};


struct VertexOut
{
	float3 PosW    : POSITION;
	float2 BoundY  : BoundY;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
	VertexOut vout;

	vout.PosW = vin.PosL + gOffsetPosition[instanceID].xyz;
	vout.PosW.y = 0.0f;
	vout.BoundY = gBoundY[instanceID].xy;

	return vout;
}