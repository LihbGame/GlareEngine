
cbuffer CubePass : register(b1)
{
    float4x4 View ;
    float4x4 Proj ;
    float4x4 ViewProj;
    float3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float cbPerObjectPad1 = 0.0f;
    float2 RenderTargetSize = { 0.0f, 0.0f };
    float2 InvRenderTargetSize = { 0.0f, 0.0f };
}


float4 main( float4 pos : POSITION ) : SV_POSITION
{
	return pos;
}