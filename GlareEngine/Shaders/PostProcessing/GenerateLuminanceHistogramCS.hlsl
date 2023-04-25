Texture2D<uint> LuminanceBuffer : register(t0);
RWByteAddressBuffer Histogram : register(u0);

groupshared uint g_TileHistogram[256];

[numthreads(16, 16, 1)]
void main(uint GI : SV_GroupIndex, uint3 DTid : SV_DispatchThreadID)
{
    g_TileHistogram[GI] = 0;

    GroupMemoryBarrierWithGroupSync();

    uint QuantizedLogLuma = LuminanceBuffer[DTid.xy];
    InterlockedAdd(g_TileHistogram[QuantizedLogLuma], 1);

    GroupMemoryBarrierWithGroupSync();

    Histogram.InterlockedAdd(GI * 4, g_TileHistogram[GI]);
}