#include "TerrainConstBuffer.hlsli"
#include "Common.hlsli"

struct VertexIn
{
	float3 PosL    : POSITION;
	float2 TexC    : TEXCOORD;
};


struct VertexOut
{
	float3 PosW    : POSITION;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	vout.PosW = vin.PosL;

	vout.PosW.y = gSRVMap[mHeightMapIndex].SampleLevel(gsamLinearWrap, vin.TexC, 0).r;

	return vout;
}