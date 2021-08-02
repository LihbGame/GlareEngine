#include "Common.hlsli"

struct VertexIn
{
	float3 PosL    : POSITION;

};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    // Transform to homogeneous clip space.
	vout.PosH = mul(posW, gViewProj);
	return vout;
}
