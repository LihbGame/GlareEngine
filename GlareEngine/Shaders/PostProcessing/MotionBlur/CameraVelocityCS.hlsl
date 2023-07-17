#include "../VelocityPacking.hlsli"

#define USE_LINEAR_Z

Texture2D<float> DepthBuffer						: register(t0);

RWTexture2D<Packed_Velocity_Type> VelocityBuffer	: register(u0);

cbuffer CBuffer                                     : register(b1)
{
    matrix CurrentToPrev;
}


[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    float2 CurrentPixel         = DTid.xy + 0.5f;
    float Depth                 = DepthBuffer[DTid.xy];

#ifdef USE_LINEAR_Z
    float4 HomogeneousPos       = float4(CurrentPixel * Depth, 1.0, Depth);
#else
    float4 HomogeneousPos       = float4(CurrentPixel, Depth, 1.0);
#endif
    float4 PrevHomogeneousPos   = mul(CurrentToPrev, HomogeneousPos);

    PrevHomogeneousPos.xyz      /= PrevHomogeneousPos.w;

#ifdef USE_LINEAR_Z
    PrevHomogeneousPos.z        = PrevHomogeneousPos.w;
#endif

    VelocityBuffer[DTid.xy]     = PackVelocity(PrevHomogeneousPos.xyz - float3(CurrentPixel, Depth));
}