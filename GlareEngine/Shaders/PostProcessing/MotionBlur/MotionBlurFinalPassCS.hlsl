//#include "../../Misc/CommonResource.hlsli"
#include "../VelocityPacking.hlsli"

#define MAX_SAMPLE_COUNT  10
#define STEP_SIZE         3.0

Texture2D<Packed_Velocity_Type>		VelocityBuffer	: register(t0);		// Full resolution motion vectors
Texture2D<float4>					PreBuffer		: register(t1);		// 1/4 resolution pre-weighted blurred color samples
RWTexture2D<float3>					DstColor		: register(u0);		// Final output color (blurred and temporally blended)
SamplerState               BiLinearClampSampler		: register(s0);


cbuffer MotionBlurConstantBuffer                    : register(b0)
{
    float2 RcpBufferDim;	// 1 / width, 1 / height
}

[numthreads(8, 8, 1)]
void main(uint3 Gid : SV_GroupID, uint GI : SV_GroupIndex, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    uint2  sp           = DTid.xy;
    float2 position     = sp + 0.5;
    float2 uv           = position * RcpBufferDim;

    float2 Velocity     = UnpackVelocity(VelocityBuffer[sp]).xy;
    float3 thisColor    = DstColor[sp];

    float   Speed       = length(Velocity);

    [branch]
    if (Speed >= 4.0)
    {
        float4 accumulate = float4(thisColor, 1);

        // Half of the speed goes in each direction
        float halfSampleCount = min(MAX_SAMPLE_COUNT * 0.5f, Speed * 0.5f / STEP_SIZE);

        float2 deltaUV = Velocity / Speed * RcpBufferDim * STEP_SIZE;
        float2 uv1 = uv;
        float2 uv2 = uv;

        // First accumulate the whole samples
        for (float i = halfSampleCount - 1.0; i > 0.0; i -= 1.0)
        {
            accumulate += PreBuffer.SampleLevel(BiLinearClampSampler, uv1 += deltaUV, 0);
            accumulate += PreBuffer.SampleLevel(BiLinearClampSampler, uv2 -= deltaUV, 0);
        }

        // This is almost the same as 'frac(halfSampleCount)' replaces 0 with 1.
        //This is to reduce sampling
        float remainder = 1 + halfSampleCount - ceil(halfSampleCount);

        // Then accumulate the fractional samples
        deltaUV *= remainder;
        accumulate += PreBuffer.SampleLevel(BiLinearClampSampler, uv1 + deltaUV, 0) * remainder;
        accumulate += PreBuffer.SampleLevel(BiLinearClampSampler, uv2 - deltaUV, 0) * remainder;

        thisColor = lerp(thisColor, accumulate.rgb / accumulate.a, saturate(Speed / 32.0));
    }

    DstColor[sp] = thisColor;
}