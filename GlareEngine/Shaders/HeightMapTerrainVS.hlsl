#include "Common.hlsli"
#include "TerrainConstBuffer.hlsli"



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