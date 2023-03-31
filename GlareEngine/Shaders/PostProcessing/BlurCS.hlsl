// Description: This is a compute shader that blurs the input texture.

#include "../Misc/CommonResource.hlsli"

Texture2D<float3> InputBuffer : register(t0);
RWTexture2D<float3> BlurResult : register(u0);

cbuffer BlurConstBuffer : register(b0)
{
    float2 g_InverseDimensions;
}

// The guassian blur weights (derived from Pascal's triangle)
static const float Weights[5] = { 70.0f / 256.0f, 56.0f / 256.0f, 28.0f / 256.0f, 8.0f / 256.0f, 1.0f / 256.0f };

float3 BlurPixels(float3 a, float3 b, float3 c, float3 d, float3 e, float3 f, float3 g, float3 h, float3 i)
{
    return Weights[0] * e + Weights[1] * (d + f) + Weights[2] * (c + g) + Weights[3] * (b + h) + Weights[4] * (a + i);
}

// 16x16 pixels to 8x8 pixels. Each uint is two color channels packed together
groupshared uint CacheR[128];
groupshared uint CacheG[128];
groupshared uint CacheB[128];


void StoreOnePixel(uint index, float3 pixel)
{
    CacheR[index] = asuint(pixel.r);
    CacheG[index] = asuint(pixel.g);
    CacheB[index] = asuint(pixel.b);
}

void LoadOnePixel(uint index, out float3 pixel)
{
    pixel = asfloat(uint3(CacheR[index], CacheG[index], CacheB[index]));
}

void StoreTwoPixels(uint index, float3 pixel1, float3 pixel2)
{
    CacheR[index] = f32tof16(pixel1.r) | f32tof16(pixel2.r) << 16;
    CacheG[index] = f32tof16(pixel1.g) | f32tof16(pixel2.g) << 16;
    CacheB[index] = f32tof16(pixel1.b) | f32tof16(pixel2.b) << 16;
}

void LoadTwoPixels(uint index, out float3 Pixel1, out float3 Pixel2)
{
    uint RR = CacheR[index];
    uint GG = CacheG[index];
    uint BB = CacheB[index];
    Pixel1 = float3(f16tof32(RR), f16tof32(GG), f16tof32(BB));
    Pixel2 = float3(f16tof32(RR >> 16), f16tof32(GG >> 16), f16tof32(BB >> 16));
}

// Blur two pixels horizontally.Reduces LDS reads and pixel unpacking.
void BlurHorizontally(uint OutIndex, uint leftMostIndex)
{
    float3 s0, s1, s2, s3, s4, s5, s6, s7, s8, s9;
    LoadTwoPixels(leftMostIndex + 0, s0, s1);
    LoadTwoPixels(leftMostIndex + 1, s2, s3);
    LoadTwoPixels(leftMostIndex + 2, s4, s5);
    LoadTwoPixels(leftMostIndex + 3, s6, s7);
    LoadTwoPixels(leftMostIndex + 4, s8, s9);

    StoreOnePixel(OutIndex, BlurPixels(s0, s1, s2, s3, s4, s5, s6, s7, s8));
    StoreOnePixel(OutIndex + 1, BlurPixels(s1, s2, s3, s4, s5, s6, s7, s8, s9));
}


void BlurVertically(uint2 pixelCoord, uint topMostIndex)
{
    float3 s0, s1, s2, s3, s4, s5, s6, s7, s8;
    LoadOnePixel(topMostIndex, s0);
    LoadOnePixel(topMostIndex + 8, s1);
    LoadOnePixel(topMostIndex + 16, s2);
    LoadOnePixel(topMostIndex + 24, s3);
    LoadOnePixel(topMostIndex + 32, s4);
    LoadOnePixel(topMostIndex + 40, s5);
    LoadOnePixel(topMostIndex + 48, s6);
    LoadOnePixel(topMostIndex + 56, s7);
    LoadOnePixel(topMostIndex + 64, s8);

    BlurResult[pixelCoord] = BlurPixels(s0, s1, s2, s3, s4, s5, s6, s7, s8);
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
    BlurVertically(DTid.xy, (GTid.y << 3) + GTid.x);
}