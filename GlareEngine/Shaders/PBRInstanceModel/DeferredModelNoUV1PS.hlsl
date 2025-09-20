#define NO_SECOND_UV 1
#include "../Shadow/RealTimeShadowHelper.hlsli"

Texture2D<float4> baseColorTexture                  : register(t20);
Texture2D<float3> metallicRoughnessTexture          : register(t21);
Texture2D<float1> occlusionTexture                  : register(t22);
Texture2D<float3> emissiveTexture                   : register(t23);
Texture2D<float3> normalTexture                     : register(t24);
Texture2D<float3> clearCoatTexture                  : register(t25);

SamplerState baseColorSampler : register(s0);
SamplerState metallicRoughnessSampler : register(s1);
SamplerState occlusionSampler : register(s2);
SamplerState emissiveSampler : register(s3);
SamplerState normalSampler : register(s4);

//Material Texture UV Flag
static const uint BASECOLOR = 0;
static const uint METALLICROUGHNESS = 1;
static const uint OCCLUSION = 2;
static const uint EMISSIVE = 3;
static const uint NORMALMAP = 4;
static const uint CLEARCOAT = 5;

//Whether to use second UV
#ifdef NO_SECOND_UV
#define UVSET( offset ) vsOutput.uv0
#else
#define UVSET( offset ) lerp(vsOutput.uv0, vsOutput.uv1, (flags >> offset) & 1)
#endif


cbuffer MaterialConstants : register(b2)
{
    float4 baseColorFactor;
    float3 emissiveFactor;
    float normalTextureScale;
    float2 metallicRoughnessFactor;
    float clearCoatFactor;
    uint shaderModelID;
    uint flags;
    uint specialflags;
}

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
#ifndef NO_TANGENT_FRAME
    float4 tangent : TANGENT;
#endif
    float2 uv0 : TEXCOORD0;
#ifndef NO_SECOND_UV
    float2 uv1 : TEXCOORD1;
#endif
    float3 worldPos : TEXCOORD2;
    float3 sunShadowCoord : TEXCOORD3;
    float3 CurPosition : TEXCOORD4;
    float3 PrePosition : TEXCOORD5;
};


float3 GetNormal(float3 normalMapSample, float3 unitNormalW, float4 tangentW)
{
    // Build orthonormal basis.
    float3 N = unitNormalW;
    float3 T = normalize(tangentW.xyz - dot(tangentW.xyz, N) * N);

    //The forward of our coordinate system is +z. If it is a -z-oriented model, 
    //use reverse coordinates to change the order of model vertices.so mul -tangentW.w
    //mul tangentW.w is for mirror mesh.
    float3 B = normalize(cross(N, T)) * (-tangentW.w);

    float3x3 TBN = float3x3(T, B, N);

    // Transform from tangent space to world space.
    float3 bumpedNormalW = mul(normalMapSample, TBN);

    return bumpedNormalW;
}


void main(in VSOutput vsOutput,
out float3 GBUFFER_Emissive : SV_Target0,
out float4 GBUFFER_Normal : SV_Target1,
out float4 GBUFFER_MSR : SV_Target2,
out float4 GBUFFER_BaseColor : SV_Target3,
out float4 GBUFFER_WorldTangent : SV_Target4,
out float2 GBUFFER_MotionVector : SV_Target5)
{
    uint2 pixelPos = uint2(vsOutput.position.xy);

    // Load and modulate textures
    float4 baseColor = baseColorFactor * baseColorTexture.SampleBias(baseColorSampler, UVSET(BASECOLOR), gMipLodBias);
    float2 metallicRoughness = metallicRoughnessFactor * metallicRoughnessTexture.SampleBias(metallicRoughnessSampler, UVSET(METALLICROUGHNESS), gMipLodBias).bg;
    float occlusion = occlusionTexture.SampleBias(occlusionSampler, UVSET(OCCLUSION), gMipLodBias);
    float3 emissive = emissiveFactor * emissiveTexture.SampleBias(emissiveSampler, UVSET(EMISSIVE), gMipLodBias);

    float3 normal = normalize(vsOutput.normal);
#ifndef NO_TANGENT_FRAME
    // Read normal map and convert to SNORM
    float3 normalMap = normalTexture.SampleBias(normalSampler, UVSET(NORMALMAP), gMipLodBias) * 2.0 - 1.0;
    // glTF spec says to normalize N before and after scaling, but that's excessive
    normalMap = normalize(normalMap * float3(normalTextureScale, normalTextureScale, 1));
    normal = GetNormal(normalMap, normal, vsOutput.tangent);
#endif
    
    //Write GBuffer
    GBUFFER_BaseColor = float4(baseColor.xyz, occlusion);
    GBUFFER_MSR = float4(metallicRoughness.x, 0.0f, metallicRoughness.y, shaderModelID);
    GBUFFER_Normal = float4((normal + 1.0f) / 2.0f, 1.0f);
    GBUFFER_Emissive = emissive;
    GBUFFER_WorldTangent = (vsOutput.tangent + 1.0f) / 2.0f;
    GBUFFER_MotionVector = ((vsOutput.PrePosition.xy / vsOutput.PrePosition.z) - (vsOutput.CurPosition.xy / vsOutput.CurPosition.z)) * float2(0.5f, -0.5f);

}