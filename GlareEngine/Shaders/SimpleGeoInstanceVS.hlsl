#include "Common.hlsli"

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float3 TangentL: TANGENT;
	float2 TexC    : TEXCOORD;
#ifdef SKINNED
	float3 BoneWeights : WEIGHTS;
	uint4 BoneIndices  : BONEINDICES;
#endif
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

#ifdef SKINNED
	float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	weights[0] = vin.BoneWeights.x;
	weights[1] = vin.BoneWeights.y;
	weights[2] = vin.BoneWeights.z;
	weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

	float3 posL = float3(0.0f, 0.0f, 0.0f);
	float3 normalL = float3(0.0f, 0.0f, 0.0f);
	float3 tangentL = float3(0.0f, 0.0f, 0.0f);
	for (int i = 0; i < 4; ++i)
	{
		// Assume no nonuniform scaling when transforming normals, so 
		// that we do not have to use the inverse-transpose.

		posL += weights[i] * mul(float4(vin.PosL, 1.0f), gBoneTransforms[vin.BoneIndices[i]]).xyz;
		normalL += weights[i] * mul(vin.NormalL, (float3x3)gBoneTransforms[vin.BoneIndices[i]]);
		tangentL += weights[i] * mul(vin.TangentL.xyz, (float3x3)gBoneTransforms[vin.BoneIndices[i]]);
	}

	vin.PosL = posL;
	vin.NormalL = normalL;
	vin.TangentL.xyz = tangentL;
#endif

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
