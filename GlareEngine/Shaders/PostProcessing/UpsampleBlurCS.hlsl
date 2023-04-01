// Description: This is a compute shader that blurs the input texture.

//The CS for combining a lower resolution bloom buffer with a higher resolution buffer
//and then guassian blurring the resultant buffer.
#include "../Misc/CommonResource.hlsli"
#include "BlurCSHelper.hlsli"


Texture2D<float3> HigherResBuffer : register(t0);
Texture2D<float3> LowerResBuffer : register(t1);
RWTexture2D<float3> BlurResult : register(u0);

SamplerState LinearBorderSampler : register(s1);

cbuffer BlurConstBuffer : register(b0)
{
    float2 g_InverseDimensions;
    float g_UpsampleBlendFactor;
}


[numthreads(8, 8, 1)]
void main(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    // Load 4 pixels per thread into LDS
    //Since the fuzzy radius is 4, subtract 4 here and take the boundary value if the subscript is negative
    int2 GroupOffsetLocation = (Gid.xy << 3) - 4;                            // Upper left pixel coordinate of group read location
    int2 ThreadOffsetLocation = (GTid.xy << 1) + GroupOffsetLocation;        // Upper left pixel coordinate of this thread will read


    // Store 4 unblurred pixels in LDS
    float2 UVUpLeft = (float2(ThreadOffsetLocation)+0.5f) * g_InverseDimensions;
    float2 UVBottomRight = UVUpLeft + g_InverseDimensions;
    float2 UVUpRight = float2(UVBottomRight.x, UVUpLeft.y);
    float2 uvBottomLeft = float2(UVUpLeft.x, UVBottomRight.y);
    int destIdx = GTid.x + (GTid.y << 4);

    float3 pixel1a = lerp(HigherResBuffer[ThreadOffsetLocation + uint2(0, 0)], LowerResBuffer.SampleLevel(LinearBorderSampler, UVUpLeft, 0.0f), g_UpsampleBlendFactor);
    float3 pixel1b = lerp(HigherResBuffer[ThreadOffsetLocation + uint2(1, 0)], LowerResBuffer.SampleLevel(LinearBorderSampler, UVUpRight, 0.0f), g_UpsampleBlendFactor);
    StoreTwoPixels(destIdx + 0, pixel1a, pixel1b);

    float3 pixel2a = lerp(HigherResBuffer[ThreadOffsetLocation + uint2(0, 1)], LowerResBuffer.SampleLevel(LinearBorderSampler, uvBottomLeft, 0.0f), g_UpsampleBlendFactor);
    float3 pixel2b = lerp(HigherResBuffer[ThreadOffsetLocation + uint2(1, 1)], LowerResBuffer.SampleLevel(LinearBorderSampler, UVBottomRight, 0.0f), g_UpsampleBlendFactor);
    StoreTwoPixels(destIdx + 8, pixel2a, pixel2b);

    GroupMemoryBarrierWithGroupSync();

    // Horizontally blur the pixels in Cache
    uint row = GTid.y << 4;
    BlurHorizontally(row + (GTid.x << 1), row + GTid.x + (GTid.x & 4));

    GroupMemoryBarrierWithGroupSync();

    // Vertically blur the pixels and write the result to memory
    BlurResult[DTid.xy] = BlurVertically((GTid.y << 3) + GTid.x);
}