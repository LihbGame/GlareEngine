#include "../Misc/CommonResource.hlsli"


cbuffer CubePass : register(b1)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float ObjectPad1 = 0.0f;
    float2 RenderTargetSize = { 0.0f, 0.0f };
    float2 InvRenderTargetSize = { 0.0f, 0.0f };
    int mSkyCubeIndex = 0;
    float mRoughness = 0.0f;
}

float4 main(PosVSOut pin) : SV_TARGET
{
      // the sample direction equals the hemisphere's orientation 
    float3 normal = normalize(pin.PosL);
    
    float3 irradiance = float3(0.0f, 0.0f, 0.0f);

    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    float sampleDelta = 0.025;
    float Samples = 0.0;
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // spherical to cartesian (in tangent space)
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            //This is a stupid way to avoid the diffuse highlights of the hdr map (Irradiance<=1.0f)
            float3 sampleColor = gCubeMaps[mSkyCubeIndex].Sample(gSamplerLinearWrap, sampleVec).rgb;
            sampleColor /= (sampleColor + float3(1.0f, 1.0f, 1.0f));
            sampleColor = pow(sampleColor, 1 / 2.2f);
            irradiance += sampleColor * sin(theta) * cos(theta);
            Samples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / float(Samples));
    
    return float4(irradiance, 1.0f);
}