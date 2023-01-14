#include "../Shadow/RealTimeShadowHelper.hlsli"

Texture2D<float4> baseColorTexture          : register(t0);
Texture2D<float3> metallicRoughnessTexture  : register(t1);
Texture2D<float1> occlusionTexture          : register(t2);
Texture2D<float3> emissiveTexture           : register(t3);
Texture2D<float3> normalTexture             : register(t4);

SamplerState baseColorSampler               : register(s0);
SamplerState metallicRoughnessSampler       : register(s1);
SamplerState occlusionSampler               : register(s2);
SamplerState emissiveSampler                : register(s3);
SamplerState normalSampler                  : register(s4);

//Material Texture UV Flag
static const uint BASECOLOR             = 0;
static const uint METALLICROUGHNESS     = 1;
static const uint OCCLUSION             = 2;
static const uint EMISSIVE              = 3;
static const uint NORMAL                = 4;

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
    uint flags;
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


float3 IBL_Diffuse(SurfaceProperties Surface)
{
   //return Surface.c_diff * irradianceIBLTexture.Sample(defaultSampler, Surface.N);

   // This is nicer but more expensive
    float LdotH = saturate(dot(Surface.N, normalize(Surface.N + Surface.V)));
    float fd90 = 0.5 + 2.0 * Surface.roughness * LdotH * LdotH;
    float3 DiffuseBurley = Surface.c_diff * Fresnel_Shlick(1, fd90, Surface.NdotV);
    return DiffuseBurley * gCubeMaps[gBakingDiffuseCubeIndex].Sample(gSamplerLinearWrap, Surface.N).rgb;
}

// Approximate specular IBL by sampling lower mips according to roughness.
float3 IBL_Specular(SurfaceProperties Surface)
{
    float lod = Surface.roughness * IBLRange + IBLBias;
    float3 F = FresnelSchlickRoughness(Surface.NdotV, Surface.c_spec, Surface.roughness);
    float3 PrefilteredColor = gCubeMaps[gBakingPreFilteredEnvIndex].SampleLevel(gSamplerLinearClamp, reflect(-Surface.V, Surface.N), lod).rgb;
    float2 BRDF = gSRVMap[gBakingIntegrationBRDFIndex].Sample(gSamplerLinearClamp, float2(Surface.NdotV, Surface.roughness)).xy;
    return PrefilteredColor * (F * BRDF.x + BRDF.y);
}


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
    // Load and modulate textures
    float4 baseColor = baseColorFactor * baseColorTexture.Sample(baseColorSampler, UVSET(BASECOLOR));
    float2 metallicRoughness = metallicRoughnessFactor * metallicRoughnessTexture.Sample(metallicRoughnessSampler, UVSET(METALLICROUGHNESS)).bg;
    float  occlusion = occlusionTexture.Sample(occlusionSampler, UVSET(OCCLUSION));
    float3 emissive = emissiveFactor * emissiveTexture.Sample(emissiveSampler, UVSET(EMISSIVE));

    float3 normal = normalize(vsOutput.normal);
#ifndef NO_TANGENT_FRAME
    // Read normal map and convert to SNORM
    float3 normalMap = normalTexture.Sample(normalSampler, UVSET(NORMAL)) * 2.0 - 1.0;
    // glTF spec says to normalize N before and after scaling, but that's excessive
    normalMap = normalize(normalMap * float3(normalTextureScale, normalTextureScale, 1));
    normal = GetNormal(normalMap, normal, vsOutput.tangent);
#endif

    SurfaceProperties Surface;
    Surface.N = normal;
    Surface.V = normalize(gEyePosW - vsOutput.worldPos);
    Surface.NdotV = saturate(dot(Surface.N, Surface.V));
    Surface.c_diff = baseColor.rgb * (1 - DielectricSpecular) * (1 - metallicRoughness.x) * occlusion;
    Surface.c_spec = lerp(DielectricSpecular, baseColor.rgb, metallicRoughness.x) * occlusion;
    Surface.roughness = metallicRoughness.y;
    Surface.alpha = metallicRoughness.y * metallicRoughness.y;
    Surface.alphaSqr = Surface.alpha * Surface.alpha;

    // Begin accumulating light starting with emissive
    float3 color = emissive;

    //Lighting and Shadow
    Surface.ShadowFactor = CalcShadowFactor(float4(vsOutput.sunShadowCoord, 1.0f));
    color += ComputeLighting(gLights, Surface);


    //SSAO (TODO)



    if (gIsIndoorScene)
    {
        //Indoor environment light(local environment light)
        color += baseColor.rgb * gAmbientLight.rgb * occlusion;
    }
    else
    {
        //Outdoor environment light (IBL)
        color += IBL_Diffuse(Surface);
        color += IBL_Specular(Surface);
    }

    // TODO: Shade each light using Forward+ tiles

    return float4(color, baseColor.a);
}