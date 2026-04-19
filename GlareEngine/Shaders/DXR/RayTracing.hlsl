//=============================================================================
// GlareEngine DXR - Single Triangle Raytracing Shader
// Shader Model 6.6
//=============================================================================

// --- Constant Buffer ---
cbuffer DXRConstants : register(b0)
{
    float4x4 InvViewProj;
    float3 CameraPos;
    float  _Pad0;
    uint   Width;
    uint   Height;
};

// --- Acceleration Structure and Output ---
RaytracingAccelerationStructure SceneAS : register(t0, space200);
RWTexture2D<float4> OutputTexture : register(u0);

// --- Payload ---
struct RayPayload
{
    float3 Color;
    float  Pad;    // align to 16 bytes to match C++ MaxPayloadSizeInBytes
};

//-----------------------------------------------------------------------------
// Ray Generation Shader
//-----------------------------------------------------------------------------
[shader("raygeneration")]
void RayGenShader()
{
    uint2 pixelCoord = DispatchRaysIndex().xy;
    uint2 dims       = DispatchRaysDimensions().xy;

    float2 ndc = (float2(pixelCoord) + 0.5) / float2(dims) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float4 origin = mul(float4(ndc, 0.0, 1.0), InvViewProj);
    float4 target = mul(float4(ndc, 1.0, 1.0), InvViewProj);
    origin.xyz /= origin.w;
    target.xyz /= target.w;

    float3 rayDir = normalize(target.xyz - origin.xyz);

    RayDesc ray;
    ray.Origin    = origin.xyz;
    ray.Direction = rayDir;
    ray.TMin      = 0.001;
    ray.TMax      = 10000.0;

    RayPayload payload;
    payload.Color = float3(0.0, 0.0, 0.0);

    TraceRay(SceneAS,
             RAY_FLAG_NONE,
             0xFF,
             0,
             1,
             0,
             ray,
             payload);

    OutputTexture[pixelCoord] = float4(payload.Color, 1.0);
}

//-----------------------------------------------------------------------------
// Closest Hit Shader
//-----------------------------------------------------------------------------
[shader("closesthit")]
void ClosestHitShader(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    float3 v0Color = float3(1.0, 0.0, 0.0);
    float3 v1Color = float3(0.0, 1.0, 0.0);
    float3 v2Color = float3(0.0, 0.0, 1.0);

    float baryW = 1.0 - attr.barycentrics.x - attr.barycentrics.y;
    payload.Color = v0Color * baryW
                  + v1Color * attr.barycentrics.x
                  + v2Color * attr.barycentrics.y;
}

//-----------------------------------------------------------------------------
// Miss Shader
//-----------------------------------------------------------------------------
[shader("miss")]
void MissShader(inout RayPayload payload)
{
    float t = 0.5 + 0.5 * WorldRayDirection().y;
    payload.Color = float3(0.1, 0.1, 0.15) * (1.0 - t) + float3(0.3, 0.3, 0.4) * t;
}
