ByteAddressBuffer Histogram : register(t0);
StructuredBuffer<float> Exposure : register(t1);
RWTexture2D<float3> ColorBuffer : register(u0);

groupshared uint gs_Histogram[256];

[numthreads(256, 1, 1)]
void main(uint GI : SV_GroupIndex, uint3 DTid : SV_DispatchThreadID)
{
    uint HistogramValue = Histogram.Load(GI * 4);

    // Compute the maximum histogram value
    gs_Histogram[GI] = (GI == 0) ? 0 : HistogramValue;
    GroupMemoryBarrierWithGroupSync();
    gs_Histogram[GI] = max(gs_Histogram[GI], gs_Histogram[(GI + 128) % 256]);
    GroupMemoryBarrierWithGroupSync();
    gs_Histogram[GI] = max(gs_Histogram[GI], gs_Histogram[(GI + 64) % 256]);
    GroupMemoryBarrierWithGroupSync();
    gs_Histogram[GI] = max(gs_Histogram[GI], gs_Histogram[(GI + 32) % 256]);
    GroupMemoryBarrierWithGroupSync();
    gs_Histogram[GI] = max(gs_Histogram[GI], gs_Histogram[(GI + 16) % 256]);
    GroupMemoryBarrierWithGroupSync();
    gs_Histogram[GI] = max(gs_Histogram[GI], gs_Histogram[(GI + 8) % 256]);
    GroupMemoryBarrierWithGroupSync();
    gs_Histogram[GI] = max(gs_Histogram[GI], gs_Histogram[(GI + 4) % 256]);
    GroupMemoryBarrierWithGroupSync();
    gs_Histogram[GI] = max(gs_Histogram[GI], gs_Histogram[(GI + 2) % 256]);
    GroupMemoryBarrierWithGroupSync();
    gs_Histogram[GI] = max(gs_Histogram[GI], gs_Histogram[(GI + 1) % 256]);
    GroupMemoryBarrierWithGroupSync();

    uint maxHistogramValue = gs_Histogram[GI];

    uint2 BufferDim;
    ColorBuffer.GetDimensions(BufferDim.x, BufferDim.y);

    const uint2 DrawRectCorner = uint2(BufferDim.x * 0.1666f, BufferDim.y *0.6f);
    const uint2 GroupCorner = DrawRectCorner + uint2(DTid.x * 4, DTid.y * 4);

    uint Height = 127 - DTid.y * 4;
    uint Threshold = HistogramValue * 128 / max(1, maxHistogramValue);

    float3 OutColor = (GI == (uint)Exposure[6]) ? float3(1.0, 1.0, 0.0) : float3(0.1, 0.5, 0.1);

    for (uint i = 0; i < 4; ++i)
    {
        if ((Height - i) < Threshold)
        {
            ColorBuffer[GroupCorner + uint2(0, i)] = OutColor;
            ColorBuffer[GroupCorner + uint2(1, i)] = OutColor;
        }
    }
}