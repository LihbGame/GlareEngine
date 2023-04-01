
// 16x16 pixels to 8x8 pixels. Each uint is two color channels packed together
groupshared uint CacheR[128];
groupshared uint CacheG[128];
groupshared uint CacheB[128];


// The guassian blur weights (derived from Pascal's triangle)
static const float Weights[5] = { 70.0f / 256.0f, 56.0f / 256.0f, 28.0f / 256.0f, 8.0f / 256.0f, 1.0f / 256.0f };

float3 BlurPixels(float3 a, float3 b, float3 c, float3 d, float3 e, float3 f, float3 g, float3 h, float3 i)
{
    return Weights[0] * e + Weights[1] * (d + f) + Weights[2] * (c + g) + Weights[3] * (b + h) + Weights[4] * (a + i);
}

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


float3 BlurVertically(uint topMostIndex)
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

    return BlurPixels(s0, s1, s2, s3, s4, s5, s6, s7, s8);
}
