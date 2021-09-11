#define    ALBEDO      0
#define    NORMAL      1
#define    AO          2
#define    METALLIC    3
#define    ROUGHNESS   4
#define    HEIGHT      5

SamplerState gsamLinearWrap : register(s0);
SamplerState gsamLinearClamp : register(s1);



// 每帧常量数据。
cbuffer MainPass : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    //float4x4 gShadowTransform;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
};
