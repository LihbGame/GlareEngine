#include "../Misc/PBRLighting.hlsli"


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
    return clamp(percentLit, 0.0f, 1.0f);
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

//Calculation of average depth of occluders
float findBlocker(Texture2D shadowMap, DiskSamples diskSamples, float texelSize, float2 uv, float zReceiver)
{
    float totalDepth = 0.0;
    int blockCount = 0;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        float2 simpleUV = uv + diskSamples.Samples[i] * texelSize*10;
        float shadowMapDepth = shadowMap.Sample(gSamplerLinearClamp, simpleUV).r;
        if (zReceiver > (shadowMapDepth + 0.00001f)) {
            totalDepth += shadowMapDepth;
            blockCount += 1;
        }
    }
    //No Occlusion
    if (blockCount == 0) {
        return -1.0;
    }
    //Fully Occlusion
    if (blockCount == NUM_SAMPLES) {
        return 2.0;
    }
    return totalDepth / float(blockCount);
}

float PCF_Internal(Texture2D shadowMap, DiskSamples diskSamples, float texelSize, float2 shadowUV, float flagDepth, float simpleScale) {
    float sum = 0.0;
    for (int i = 0; i < NUM_SAMPLES; i++) 
    {
        float2 simpleUV = shadowUV + diskSamples.Samples[i] * texelSize * simpleScale;
        float shadowMapDepth = shadowMap.SampleCmpLevelZero(gSamplerShadow, simpleUV, flagDepth).r;
        sum += shadowMapDepth;
    }
    return sum / float(NUM_SAMPLES);
}


//The penumbra diameter is calculated using similar triangles and passed to the PCF function to adjust its filter kernel size.
float PCSS(Texture2D shadowMap, float4 coords) 
{
    coords.xyz /= coords.w;
    //caculate poisson disk samples
    DiskSamples poissonDisk = poissonDiskSamples(coords.xy);

    float lightSize = 30;
        
    uint width, height, numMips;
    shadowMap.GetDimensions(0, width, height, numMips);
    // Texel size.
    float dx = 1.0f / (float)width;

    // STEP 1: avg blocker depth
    float zBlocker = findBlocker(shadowMap, poissonDisk, dx, coords.xy, coords.z);
    if (zBlocker < EPS)
    { //No Occlusion   
        return 1.0;
    }

    if (zBlocker > 1.0 + EPS) {
        return 0.0;
    }

    // STEP 2: penumbra size 
    float penumbraScale = (coords.z - zBlocker) / zBlocker;


    // STEP 3: filtering  
    return PCF_Internal(shadowMap, poissonDisk, dx, coords.xy, coords.z, penumbraScale* lightSize);

}