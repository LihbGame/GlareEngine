#include "../Misc/CommonResource.hlsli"



//---------------------------------------------------------------------------------------
// PCF for shadow mapping.
//---------------------------------------------------------------------------------------

//PCF box filter
float CalcShadowFactor(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;

    uint width, height, numMips;
    gSRVMap[48].GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float)width;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += gSRVMap[gShadowMapIndex].SampleCmpLevelZero(gSamplerShadow,
            shadowPosH.xy + offsets[i], depth).r;
    }
    percentLit /= 9.0f;
    return clamp(percentLit, 0.1f, 1.0f);
}

float PCF(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;

    uint width, height, numMips;
    gSRVMap[gShadowMapIndex].GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float)width;


    float  visibility = 0.0;

    float  filterSize = dx;
    //caculate poisson disk samples
    DiskSamples poissonDisk = poissonDiskSamples(shadowPosH.xy);

    [unroll]
    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        float2  texcoords = poissonDisk.Samples[i] * filterSize + shadowPosH.xy;
        visibility += gSRVMap[gShadowMapIndex].SampleCmpLevelZero(gSamplerShadow,
            texcoords, depth).r;
    }

    return visibility / float(NUM_SAMPLES);
}