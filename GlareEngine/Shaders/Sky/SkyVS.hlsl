#include "../Misc/CommonResource.hlsli"


cbuffer SkyPass : register(b1)
{
    float4x4 gWorld;
}


PosVSOut main(float3 PosL : POSITION)
{
    PosVSOut vout;

	// Use local vertex positions as cubemap sampling vectors.
    vout.PosL = PosL;

	// Convert to world space
    float4 posW = mul(float4(PosL, 1.0f), gWorld);

	//Fix the skybox to the eye position
    posW.xyz += gEyePosW;

	// Set z = w so that z/w = 1 (i.e. the celestial dome is always on the far plane).
    vout.PosH = mul(posW, gViewProj).xyww;

    return vout;
}