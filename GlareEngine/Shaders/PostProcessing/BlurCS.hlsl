// Description: This is a compute shader that blurs the input texture.

#include "../Misc/CommonResource.hlsli"
#include "BlurCSHelper.hlsli"


Texture2D<float3> InputBuffer : register(t0);
RWTexture2D<float3> BlurResult : register(u0);

cbuffer BlurConstBuffer : register(b0)
{
    float2 g_InverseDimensions;
}

[numthreads(8, 8, 1)]
void main(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    // Load 4 pixels per thread into LDS
    //Since the fuzzy radius is 4, subtract 4 here and take the boundary value if the subscript is negative
    int2 GroupOffsetLocation = (Gid.xy << 3) - 4;                            // Upper left pixel coordinate of group read location
    int2 ThreadOffsetLocation = (GTid.xy << 1) + GroupOffsetLocation;        // Upper left pixel coordinate of this thread will read

    // Store 4 unblurred pixels in LDS
    int destIdx = GTid.x + (GTid.y << 4);
    StoreTwoPixels(destIdx + 0, InputBuffer[ThreadOffsetLocation + uint2(0, 0)], InputBuffer[ThreadOffsetLocation + uint2(1, 0)]);
    StoreTwoPixels(destIdx + 8, InputBuffer[ThreadOffsetLocation + uint2(0, 1)], InputBuffer[ThreadOffsetLocation + uint2(1, 1)]);

    GroupMemoryBarrierWithGroupSync();

    // Horizontally blur the pixels in Cache
    uint row = GTid.y << 4;
    BlurHorizontally(row + (GTid.x << 1), row + GTid.x + (GTid.x & 4));

    GroupMemoryBarrierWithGroupSync();

    // Vertically blur the pixels and write the result to memory
    BlurResult[DTid.xy] = BlurVertically((GTid.y << 3) + GTid.x);
}