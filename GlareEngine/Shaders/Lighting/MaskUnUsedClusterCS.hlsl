
Texture2D LinearDepthTex : register(t0);
RWStructuredBuffer<float> ActiveClusterList : register(u0);

SamplerState LinearClampSample : register(s0);

cbuffer CSConstant : register(b0)
{
    float2 cluserFactor;
    float2 perTileSize;
    float3 tileSizes;
    float ScreenWidth;
    float ScreenHeight;
    float farPlane;
    float nearPlane;
};

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 screenPos = uint2(dispatchThreadId.x, dispatchThreadId.y);
    float depth = LinearDepthTex[screenPos].r;
    float viewZ = depth * farPlane;
    uint clusterZ = uint(max(log2(viewZ) * cluserFactor.x + cluserFactor.y, 0.0));
    uint3 clusters = uint3(uint(screenPos.x / perTileSize.x), uint(screenPos.y / perTileSize.y), clusterZ);
    uint clusterIndex = clusters.x + clusters.y * (uint) tileSizes.x + clusters.z * (uint) tileSizes.x * (uint) tileSizes.y;
    ActiveClusterList[clusterIndex] = 1.0;
}