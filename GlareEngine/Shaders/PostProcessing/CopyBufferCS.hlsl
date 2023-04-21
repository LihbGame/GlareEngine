RWTexture2D<float3> ResultBuffer : register(u0);
Texture2D<float3> SourceBuffer : register(t0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    ResultBuffer[DTid.xy] = SourceBuffer[DTid.xy];
}