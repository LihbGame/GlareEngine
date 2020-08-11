#include "Common.hlsli"

struct VertexIn
{
	float3 PosL    : POSITION;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
	VertexOut vout = (VertexOut)0.0f;

	// Fetch the instance data.
	InstanceData instData = gInstanceData[instanceID];
	float4x4 world = instData.World;
	float4x4 texTransform = instData.TexTransform;



	// Transform to world space.
	float4 posW = mul(float4(vin.PosL, 1.0f), world);
	// Transform to homogeneous clip space.
	vout.PosH = mul(posW, gViewProj);

	// Output vertex attributes for interpolation across triangle.
	float2 texC = mul(float4(vin.TexC, 0.0f, 1.0f), texTransform).xy;

	return vout;
}
