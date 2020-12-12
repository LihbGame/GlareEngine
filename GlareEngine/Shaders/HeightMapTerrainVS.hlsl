#include "Common.hlsli"
#include "TerrainConstBuffer.hlsli"

struct VertexIn
{
	float3 PosL     : POSITION;
	float2 Tex      : TEXCOORD;
	float2 BoundsY  : BoundY;
};

struct VertexOut
{
	float3 PosW     : POSITION;
	float2 Tex      : TEXCOORD;
	float2 BoundsY  : BoundY;
};


VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	// Terrain specified directly in world space.
	vout.PosW = vin.PosL;

	//将patch corners移到世界空间。 这是为了使眼睛到patch的距离计算更加准确。
	vout.PosW.y = gSRVMap[mHeightMapIndex].SampleLevel(gsamLinearWrap, vin.Tex, 0).r;

	// Output vertex attributes to next stage.
	vout.Tex = vin.Tex;
	vout.BoundsY = vin.BoundsY;

	return vout;
}