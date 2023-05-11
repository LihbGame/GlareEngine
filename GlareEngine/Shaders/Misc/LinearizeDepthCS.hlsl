RWTexture2D<float> LinearZ : register(u0);
Texture2D<float> Depth : register(t0);

cbuffer CB0 : register(b0)
{
    float ZMagic;                // (zFar - zNear) / zNear
}

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    LinearZ[DTid.xy] = 1.0 / (ZMagic * Depth[DTid.xy] + 1.0);
}
