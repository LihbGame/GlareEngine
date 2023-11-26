Texture2D<float4> TemporalColor : register(t0);
RWTexture2D<float3> OutColor : register(u0);


[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float4 Color = TemporalColor[DTid.xy];
    OutColor[DTid.xy] = Color.rgb / max(Color.w, 1e-6);
}