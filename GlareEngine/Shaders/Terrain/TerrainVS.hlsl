#include "../Misc/CommonResource.hlsli"

struct VertexIn
{
	float3 PosL : POSITION;
	float2 TexC : TEXCOORD;
};

struct VertexOut
{
	float2 TexC : TEXCOORD;
	nointerpolation uint MatIndex : MATINDEX;
};

VertexOut main(VertexIn vin)
{
	VertexOut vout;

	// Terrain specified directly in world space.
	vout.MatIndex = 0;

	// Output vertex attributes to next stage.
	vout.TexC = vin.TexC;

	return vout;
}