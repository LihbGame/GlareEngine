
ByteAddressBuffer Histogram : register(t0);
RWStructuredBuffer<float> Exposure : register(u0);

cbuffer AdaptationConstant : register(b1)
{
    float   TargetLuminance;
    float   AdaptationTranform;
    float   MinExposure;
    float   MaxExposure;
    uint    PixelCount;
}

groupshared float gs_Accum[256];

[numthreads(256, 1, 1)]
void main(uint GI : SV_GroupIndex)
{
    //The brighter the pixel, the higher the weighting
    float WeightedSum = (float)GI * (float)Histogram.Load(GI * 4);

    //Parallel optimization to reduce atomic synchronization
    [unroll]
    for (uint i = 1; i < 256; i *= 2)
    {
        gs_Accum[GI] = WeightedSum;                 
        GroupMemoryBarrierWithGroupSync();          
        WeightedSum += gs_Accum[(GI + i) % 256];    
        GroupMemoryBarrierWithGroupSync();          
    }

    // If the entire image is black, don't adjust exposure
    if (WeightedSum == 0.0)
        return;

    float MinLog        = Exposure[2];
    float MaxLog        = Exposure[3];
    float LogRange      = Exposure[4];
    float RcpLogRange   = Exposure[5];
    
    //weightedHistAvg (0-255)
    float weightedHistAvg = WeightedSum / (max(1, PixelCount - Histogram.Load(0))) - 1.0;
    //Luminance log Avg
    float logAvgLuminance = exp2(weightedHistAvg / 254.0 * LogRange + MinLog);
    //The brighter it is, the smaller it is
    float targetExposure = TargetLuminance / logAvgLuminance;

    float exposure = Exposure[0];
    //Transition per frame, adjust exposure
    exposure = lerp(exposure, targetExposure, AdaptationTranform);
    exposure = clamp(exposure, MinExposure, MaxExposure);

    if (GI == 0)
    {
        Exposure[0] = exposure;
        Exposure[1] = 1.0 / exposure;

        // First attempt to recenter our histogram around the log-average.
        float BiasToCenter = (floor(weightedHistAvg) - 128.0) / 255.0;
        if (abs(BiasToCenter) > 0.1f)
        {
            MinLog += BiasToCenter * RcpLogRange;
            MaxLog += BiasToCenter * RcpLogRange;
        }

        Exposure[2] = MinLog;
        Exposure[3] = MaxLog;
        Exposure[4] = LogRange;
        Exposure[5] = 1.0 / LogRange;
        //For Histogram Debug Draw
        Exposure[6] = weightedHistAvg;
    }
}