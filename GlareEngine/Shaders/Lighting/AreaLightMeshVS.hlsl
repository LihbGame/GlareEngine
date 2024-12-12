#include "../Misc/CommonResource.hlsli"

StructuredBuffer<AreaLightInstanceData> AreaInstanceData : register(t0, space1);

PosNorTanTexInstanceOut main(PosNorTanTexIn vin, uint instanceID : SV_InstanceID)
{
    PosNorTanTexInstanceOut vout = (PosNorTanTexInstanceOut) 0.0f;

	// Fetch the instance data.
    AreaLightInstanceData instData = AreaInstanceData[instanceID];
    float4x4 world = instData.World;

	//PS NEED
    vout.NormalW = mul(vin.NormalL, (float3x3) world);
    vout.TangentW = mul(vin.TangentL, (float3x3) world);

	// Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), world);
    vout.PosW = posW.xyz;
	// Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);

	// Output vertex attributes for interpolation across triangle.
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), instData.TexTransform).xy;
    
	// Generate projective tex-coords to project shadow map onto scene.
    vout.ShadowPosH = mul(posW, gShadowTransform);
    
    vout.InstanceIndex = instanceID;
    return vout;
}