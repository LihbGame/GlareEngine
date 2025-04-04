#include "../Misc/CommonResource.hlsli"

cbuffer CubePass : register(b1)
{
    float4x4 View ;
    float4x4 Proj ;
    float4x4 ViewProj;
    float3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float ObjectPad1 = 0.0f;
    float2 RenderTargetSize = { 0.0f, 0.0f };
    float2 InvRenderTargetSize = { 0.0f, 0.0f };
    int mSkyCubeIndex = 0;
    float mRoughness = 0.0f;
}


PosVSOut main(float3 PosL : POSITION)
{
    PosVSOut vout;

	//Use the local vertex position as the cubemap sample vector.
    vout.PosL = PosL;
	//Set z = w so that z / w = 1 (i.e. the skydome is always on the far plane).
    vout.PosH = mul(float4(PosL,1.0f), ViewProj).xyww;

    return vout;
}