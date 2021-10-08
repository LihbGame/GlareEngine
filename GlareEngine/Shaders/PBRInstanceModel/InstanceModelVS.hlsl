#include "../Misc/CommonResource.hlsli"

StructuredBuffer<MaterialData> gMaterialData : register(t1, space1);
StructuredBuffer<InstanceData> gInstanceData : register(t0, space1);


PosNorTanTexOut main(PosNorTanTexIn vin, uint instanceID : SV_InstanceID)
{
    PosNorTanTexOut vout = (PosNorTanTexOut) 0.0f;

	// Fetch the instance data.
    InstanceData instData = gInstanceData[instanceID];
    float4x4 world = instData.World;
    uint MatIndex = instData.MaterialIndex;

    vout.MatIndex = MatIndex;

	// Fetch the material data.
    MaterialData matData = gMaterialData[MatIndex];

	//PS NEED
    vout.NormalW = mul(vin.NormalL, (float3x3) world);
    vout.TangentW = mul(vin.TangentL, (float3x3) world);

	// Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), world);
    vout.PosW = posW.xyz;
	// Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);

	// Output vertex attributes for interpolation across triangle.
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), matData.mMatTransform).xy;

	// Generate projective tex-coords to project shadow map onto scene.
    //vout.ShadowPosH = mul(posW, gShadowTransform);
    return vout;
}