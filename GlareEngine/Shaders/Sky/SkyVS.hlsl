#include "../Misc/CommonResource.hlsli"


//Position
struct SkyVSOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : POSITION0;
    float3 CurPosition : TEXCOORD0;
    float3 PrePosition : TEXCOORD1;
};

cbuffer SkyPass : register(b1)
{
    float4x4 gWorld;
}


SkyVSOut main(float3 PosL : POSITION)
{
    SkyVSOut vout;

	// Use local vertex positions as cubemap sampling vectors.
    vout.PosL = PosL;

	// Convert to world space.
    float4 posW = mul(float4(PosL, 1.0f), gWorld);
    float4 prevPosW = posW;

	// Fix the skybox to the current and previous eye positions.
    posW.xyz += gEyePosW;
    prevPosW.xyz += gPrevEyePosW;

	// Set z = w so that z/w = 1 (i.e. the celestial dome is always on the far plane).
    vout.PosH = mul(posW, gViewProj).xyww;
    float4 preposition = mul(prevPosW, gPreViewProjMatrix);
    vout.CurPosition = vout.PosH.xyw;
    vout.PrePosition = preposition.xyw;
    
    return vout;
}
