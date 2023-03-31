// Description: This is a compute shader that blurs the input texture.

Texture2D<float3> HigherResBuffer : register(t0);
Texture2D<float3> LowerResBuffer : register(t1);
RWTexture2D<float3> BlurResult : register(u0);

SamplerState LinearBorderSampler : register(s1);

cbuffer BlurConstBuffer : register(b0)
{
    float2 g_InverseDimensions;
    float g_UpsampleBlendFactor;
}

// The guassian blur weights (derived from Pascal's triangle)
static const float Weights5[3] = { 6.0f / 16.0f, 4.0f / 16.0f, 1.0f / 16.0f };
static const float Weights7[4] = { 20.0f / 64.0f, 15.0f / 64.0f, 6.0f / 64.0f, 1.0f / 64.0f };
static const float Weights9[5] = { 70.0f / 256.0f, 56.0f / 256.0f, 28.0f / 256.0f, 8.0f / 256.0f, 1.0f / 256.0f };

float3 Blur5(float3 a, float3 b, float3 c, float3 d, float3 e, float3 f, float3 g, float3 h, float3 i)
{
    return Weights5[0] * e + Weights5[1] * (d + f) + Weights5[2] * (c + g);
}

float3 Blur7(float3 a, float3 b, float3 c, float3 d, float3 e, float3 f, float3 g, float3 h, float3 i)
{
    return Weights7[0] * e + Weights7[1] * (d + f) + Weights7[2] * (c + g) + Weights7[3] * (b + h);
}

float3 Blur9(float3 a, float3 b, float3 c, float3 d, float3 e, float3 f, float3 g, float3 h, float3 i)
{
    return Weights9[0] * e + Weights9[1] * (d + f) + Weights9[2] * (c + g) + Weights9[3] * (b + h) + Weights9[4] * (a + i);
}




[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
}