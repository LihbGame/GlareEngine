#include "../Shadow/RealTimeShadowHelper.hlsli"

Texture2D<float3> GBUFFER_Emissive      : register(t0);
Texture2D<float4> GBUFFER_Normal        : register(t1);
Texture2D<float4> GBUFFER_MSR           : register(t2);
Texture2D<float4> GBUFFER_BaseColor     : register(t3);
Texture2D<float4> GBUFFER_WorldTangent  : register(t4);
Texture2D<float>  DepthTexture          : register(t5);

RWTexture2D<float3> OutColor            : register(u0);

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint2 pixelPos = DTid.xy;
    //Material mask
    if (GBUFFER_MSR[pixelPos].a==1)
    {
        float3 normal = GBUFFER_Normal[pixelPos].xyz * 2.0f - 1.0f;
        float3 emissive = GBUFFER_Emissive[pixelPos].xyz;
        float3 metallicSpecRoughness = GBUFFER_MSR[pixelPos].xyz;
        float4 baseColorAndAO = GBUFFER_BaseColor[pixelPos];
        float3 worldPos = Reconstruct_Position(pixelPos * gInvRenderTargetSize, DepthTexture[pixelPos], gInvViewProj);
        float3 sunShadowCoord = mul(float4(worldPos, 1.0), gShadowTransform).xyz;

        //metallicSpecRoughness.z = 0.1f;
        //metallicSpecRoughness.x = 0.9f;

        SurfaceProperties Surface;
        Surface.N = normalize(normal);
        Surface.V = normalize(gEyePosW - worldPos);
        Surface.worldPos = worldPos;
        Surface.NdotV = saturate(dot(Surface.N, Surface.V));
        Surface.c_diff = baseColorAndAO.rgb * (1 - metallicSpecRoughness.x);
        Surface.c_spec = lerp(DielectricSpecular, baseColorAndAO.rgb, metallicSpecRoughness.x);
        Surface.roughness = metallicSpecRoughness.z;
        Surface.alpha = metallicSpecRoughness.z * metallicSpecRoughness.z;
        Surface.alphaSqr = Surface.alpha * Surface.alpha;
        Surface.ao = baseColorAndAO.a;

        // Begin accumulating light starting with emissive
        float3 color = emissive;

        //Lighting and Shadow
        Surface.ShadowFactor = CalcShadowFactor(float4(sunShadowCoord, 1.0f));
        color += ComputeLighting(gLights, Surface);
   
        //SSAO
        float ssao = gSsaoTex[pixelPos];

        Surface.ao = min(Surface.ao, ssao);

        if (gIsRenderTiledBaseLighting)
        {
            if (!gIsClusterBaseLighting)
            {
                //Shading each light using Tiled Base
                color += ComputeTiledLighting(pixelPos, Surface);
            }
            else
            {
                //Shading each light using Cluster Base
                float viewZ = (1.0 / (gZMagic * DepthTexture[pixelPos] + 1.0)) * gFarZ;
                color += ComputeClusterLighting(pixelPos, viewZ, Surface);
            }
        }
        
        if (gIsIndoorScene)
        {
            //Indoor environment light(local environment light)
            color += baseColorAndAO.rgb * gAmbientLight.rgb * Surface.ao;
        }
        else
        {
            Surface.c_diff *= Surface.ao;
            Surface.c_spec *= Surface.ao;
            //Outdoor environment light (IBL)
            color += IBL_Diffuse(Surface);
            color += IBL_Specular(Surface);
        }

        OutColor[pixelPos] = color;
    }
}