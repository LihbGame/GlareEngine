// The CS for downsampling 16x16 blocks of pixels down to 4x4 and 1x1 blocks.

Texture2D<float3> BloomBuffer : register(t0);
RWTexture2D<float3> DownSampleResult4x4 : register(u0);
RWTexture2D<float3> DownSampleResult1x1 : register(u1);

SamplerState BiLinearClampSampler: register(s0);

cbuffer ConstantBuffer : register(b0)
{
    float2 g_InverseDimensions;
}

groupshared float3 g_Tile[64];    // 8x8 input pixels

[numthreads(8, 8, 1)]
void main(uint GI : SV_GroupIndex, uint3 DTid : SV_DispatchThreadID)
{
     // If both x and y are divisible by a power of two 
    uint Parity = DTid.x | DTid.y;

    // DownSample and store the 8x8 block
    float2 CenterUV = (float2(DTid.xy) * 2.0f + 1.0f) * g_InverseDimensions;
    float3 AvgPixel = BloomBuffer.SampleLevel(BiLinearClampSampler, CenterUV, 0.0f);
    g_Tile[GI] = AvgPixel;

    GroupMemoryBarrierWithGroupSync();

    // DownSample and store the 4x4 block
    if ((Parity & 1) == 0)
    {
        AvgPixel = 0.25f * (AvgPixel + g_Tile[GI + 1] + g_Tile[GI + 8] + g_Tile[GI + 9]);
        g_Tile[GI] = AvgPixel;
        DownSampleResult4x4[DTid.xy >> 1] = AvgPixel;
    }

    GroupMemoryBarrierWithGroupSync();

    // Downsample and store the 2x2 block
    if ((Parity & 3) == 0)
    {
        AvgPixel = 0.25f * (AvgPixel + g_Tile[GI + 2] + g_Tile[GI + 16] + g_Tile[GI + 18]);
        g_Tile[GI] = AvgPixel;
    }

    GroupMemoryBarrierWithGroupSync();

    // Downsample and store the 1x1 block
    if ((Parity & 7) == 0)
    {
        AvgPixel = 0.25f * (AvgPixel + g_Tile[GI + 4] + g_Tile[GI + 32] + g_Tile[GI + 36]);
        DownSampleResult1x1[DTid.xy >> 3] = AvgPixel;
    }

}