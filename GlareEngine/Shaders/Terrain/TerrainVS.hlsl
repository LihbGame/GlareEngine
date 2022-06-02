#include "../Misc/CommonResource.hlsli"

VertexOut main(VertexIn vin)
{
	VertexOut vout;

	// Terrain specified directly in world space.
	vout.PosW = vin.PosL;

	//Moves patch corners to world space. This is to make the eye-to-patch distance calculation more accurate.
	vout.PosW.y = gSRVMap[gHeightMapIndex].SampleLevel(gSamplerLinearWrap, vin.Tex, 0).r;

	// Output vertex attributes to next stage.
	vout.Tex = vin.Tex;
	vout.BoundsY = vin.BoundsY;

	return vout;
}