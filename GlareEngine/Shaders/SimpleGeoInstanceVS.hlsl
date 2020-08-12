#include "Common.hlsli"

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float3 TangentL: TANGENT;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float4 ShadowPosH : POSITION0;
	float3 PosW    : POSITION1;
	float3 NormalW : NORMAL;
	float3 TangentW:TANGENT;
	float2 TexC    : TEXCOORD;
	nointerpolation uint MatIndex  : MATINDEX;
};



VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
	VertexOut vout = (VertexOut)0.0f;

	// Fetch the instance data.
	InstanceData instData = gInstanceData[instanceID];
	float4x4 world = instData.World;
	float4x4 texTransform = instData.TexTransform;
	uint matIndex = instData.MaterialIndex;

	vout.MatIndex = matIndex;

	// Fetch the material data.
	MaterialData matData = gMaterialData[matIndex];

	//PS NEED
	vout.NormalW = mul(vin.NormalL, (float3x3)world);
	vout.TangentW = mul(vin.TangentL, (float3x3)world);

	// Transform to world space.
	float4 posW = mul(float4(vin.PosL, 1.0f), world);
	vout.PosW = posW.xyz;
	// Transform to homogeneous clip space.
	vout.PosH = mul(posW, gViewProj);


	// Output vertex attributes for interpolation across triangle.
	float2 texC= mul(float4(vin.TexC, 0.0f, 1.0f), texTransform).xy;
	vout.TexC = mul(float4(texC, 0.0f, 1.0f),matData.MatTransform).xy;

	// Generate projective tex-coords to project shadow map onto scene.
	vout.ShadowPosH = mul(posW, gShadowTransform);
	return vout;
}
