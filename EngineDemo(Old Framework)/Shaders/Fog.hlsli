#include "Common.hlsli"
/*Fog*//////////////////////
//Linear Fog
float LinearFog(float distToEye)
{
    float fogLerp = saturate((distToEye - gFogStart) / gFogRange);
    return fogLerp;
}

//Exponential Fog
float ExponentialFog(float Density, float distToEye)
{
    return 1 - exp(-(distToEye / gFogRange) * Density);
}

//Exponential Squared Fog
float ExponentialSquaredFog(float Density, float distToEye)
{
    float dist = distToEye / gFogRange;
    float factor = dist * Density;
    return 1 - exp(-factor * factor);
}

//Layered Fog
float LayeredFog(float FogEnd, float FogRange, float FogTop, float3 PosW)
{
    // Scaled distance calculation in x-z plane
    float fDeltaD = length(gEyePosW - PosW) / FogEnd;
    
    // Height-based calculations
    float fDeltaY, fDensityIntegral;
    if (gEyePosW.y > FogTop)
    {
        if (PosW.y < FogTop)
        {
            fDeltaY = (FogTop - PosW.y) / FogRange;
            fDensityIntegral = (fDeltaY * fDeltaY);
        }
        else
        {
            fDeltaY = 0.0f;
            fDensityIntegral = 0.0f;
        }
    }
    else
    {
        if (PosW.y < FogTop)
        {
            float fDeltaA = (FogTop - gEyePosW.y) / FogRange;
            float fDeltaB = (FogTop - PosW.y) / FogRange;
            fDeltaY = abs(fDeltaA - fDeltaB);
            fDensityIntegral = abs((fDeltaA * fDeltaA) - (fDeltaB * fDeltaB));
        }
        else
        {
            fDeltaY = abs(FogTop - gEyePosW.y) / FogRange;
            fDensityIntegral = abs(fDeltaY * fDeltaY);
        }
    }
    float fDensity;
    if (fDeltaY != 0.0f)
    {
        fDensity = (sqrt(1.0f + ((fDeltaD / fDeltaY) * (fDeltaD / fDeltaY)))) * fDensityIntegral;
    }
    else
    {
        fDensity = 0.0f;
    }
 
    return 1 - exp(-fDensity);
}