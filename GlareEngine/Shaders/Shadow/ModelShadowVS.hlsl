#include "../Misc/CommonResource.hlsli"

cbuffer ShadowConstant : register(b1)
{
    float4x4 gShadowViewProj;
}

struct VertexIn
{
    float3 PosL : POSITION;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
    nointerpolation uint MatIndex : MATINDEX;
};


VertexOut main(VertexIn vin, uint instanceID : SV_InstanceID)
{
    VertexOut vout = (VertexOut) 0.0f;

	// Fetch the instance data.
    InstanceData instData = gInstanceData[instanceID];

    vout.MatIndex = instData.MaterialIndex;
	// Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), instData.World);
	// Transform to homogeneous clip space.
    vout.PosH = mul(posW, gShadowViewProj);
	// Output vertex attributes for interpolation across triangle.
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), instData.TexTransform).xy;

    return vout;
}