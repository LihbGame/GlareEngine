#define NO_SECOND_UV 1
#include "../Shadow/RealTimeShadowHelper.hlsli"

Texture2D<float4> baseColorTexture          : register(t20);
Texture2D<float3> metallicRoughnessTexture  : register(t21);
Texture2D<float1> occlusionTexture          : register(t22);
Texture2D<float3> emissiveTexture           : register(t23);
Texture2D<float3> normalTexture             : register(t24);
Texture2D<float3> clearCoatTexture          : register(t25);

//FSR MASK
RWTexture2D<float> transparentMask          : register(u0);
RWTexture2D<float> reactiveMask             : register(u1);

SamplerState baseColorSampler               : register(s0);
SamplerState metallicRoughnessSampler       : register(s1);
SamplerState occlusionSampler               : register(s2);
SamplerState emissiveSampler                : register(s3);
SamplerState normalSampler                  : register(s4);

//Material Texture UV Flag
static const uint BASECOLOR = 0;
static const uint METALLICROUGHNESS = 1;
static const uint OCCLUSION = 2;
static const uint EMISSIVE = 3;
static const uint NORMALMAP = 4;
static const uint CLEARCOAT = 5;
//Material blend mask
static const uint ALPHABLEND = 7;

//Whether to use second UV
#ifdef NO_SECOND_UV
#define UVSET( offset ) vsOutput.uv0
#else
#define UVSET( offset ) lerp(vsOutput.uv0, vsOutput.uv1, (flags >> offset) & 1)
#endif


cbuffer MaterialConstants : register(b2)
{
    float4  baseColorFactor;
    float3  emissiveFactor;
    float   normalTextureScale;
    float2  metallicRoughnessFactor;
    float   clearCoatFactor;
    uint    shaderModelID;
    uint    flags;
    uint    specialflags;
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


float4 main(VSOutput vsOutput) : SV_Target0
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

     SurfaceProperties Surface;
     Surface.N = normal;
     Surface.V = normalize(gEyePosW - vsOutput.worldPos);    
     Surface.worldPos = vsOutput.worldPos;
     Surface.NdotV = saturate(dot(Surface.N, Surface.V));
     Surface.c_diff = baseColor.rgb * (1 - metallicRoughness.x);
     Surface.c_spec = lerp(DielectricSpecular, baseColor.rgb, metallicRoughness.x);
     Surface.roughness = metallicRoughness.y;
     Surface.alpha = metallicRoughness.y * metallicRoughness.y;
     Surface.alphaSqr = Surface.alpha * Surface.alpha;
     Surface.ao = occlusion;

     // Begin accumulating light starting with emissive
     float3 color = emissive;

     //Lighting and Shadow
     Surface.ShadowFactor = CalcShadowFactor(float4(vsOutput.sunShadowCoord, 1.0f));
     color += ComputeLighting(gLights, Surface);

     //SSAO
     float ssao = gSsaoTex.SampleLevel(baseColorSampler, gInvRenderTargetSize * pixelPos, 0);

     if (gIsIndoorScene)
     {
        if (gIsRenderTiledBaseLighting)
        {
            //Shade each light using Forward+ tiles
            color += ComputeTiledLighting(pixelPos, Surface);
        }
         //Indoor environment light(local environment light)
         color += baseColor.rgb * gAmbientLight.rgb * ssao * occlusion;
     }
     else
     {
         Surface.c_diff *= ssao;
         Surface.c_spec *= ssao;
         //Outdoor environment light (IBL)
         color += IBL_Diffuse(Surface);
         color += IBL_Specular(Surface);
     }

    if ((flags >> ALPHABLEND) & 1)
    {
        transparentMask[pixelPos] += (1.0f - transparentMask[pixelPos]) * baseColor.a;
    }
    
    return float4(color, baseColor.a);
}