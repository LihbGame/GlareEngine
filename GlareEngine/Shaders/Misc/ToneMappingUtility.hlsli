#include "ShaderUtility.hlsli"

#ifndef TONE_MAPPING_UTILITY
#define TONE_MAPPING_UTILITY

//
// Reinhard
// 

// The Reinhard tone operator.  Typically, the value of k is 1.0, but you can adjust exposure by 1/k.
// I.e. TM_Reinhard(x, 0.5) == TM_Reinhard(x * 2.0, 1.0)
float3 TM_Reinhard(float3 hdr, float k = 1.0)
{
    return hdr / (hdr + k);
}

// The inverse of Reinhard
float3 ITM_Reinhard(float3 sdr, float k = 1.0)
{
    return k * sdr / (k - sdr);
}

//
// Reinhard-Squared
//

//This has some nice properties that improve upon the basic Reinhard.
//First, it has a "toe" - nice parabolic rise that enhances contrast and color saturation in darks.
//Second, it has a long shoulder that gives more detail in the highlights and takes longer to desaturate.
//It's reversible, scales to HDR displays, and is easy to control.
//
//The default constant of 0.25 was chosen for two reasons. It's very close to the effect of Reinhard, with a constant of 1.0.
//And with a constant of 0.25, there's an inflection point at 0.25 where the curve touches the line y=x and then the shoulder begins.
//
// Note: If you're currently using ACES and you prescale by 0.6, then k=0.30 looks good as an alternative without any other adjustments.
float3 TM_ReinhardSq(float3 hdr, float k = 0.25)
{
    float3 reinhard = hdr / (hdr + k);
    return reinhard * reinhard;
}

float3 ITM_ReinhardSq(float3 sdr, float k = 0.25)
{
    return k * (sdr + sqrt(sdr)) / (1.0 - sdr);
}

//
// Stanard (New)
//

// This is the new tone operator. It is similar to ACES in many ways, but is simpler to evaluate using the ALU.
// One advantage over Reinhard-Squared is that the shoulders turn to white sooner, giving the image higher overall brightness and contrast.
float3 TM_Stanard(float3 hdr)
{
    return TM_Reinhard(hdr * sqrt(hdr), sqrt(4.0 / 27.0));
}

float3 ITM_Stanard(float3 sdr)
{
    return pow(ITM_Reinhard(sdr, sqrt(4.0 / 27.0)), 2.0 / 3.0);
}

//
// Stanard (Old)
//

//This is the old tone operator first used in HemiEngine and MiniEngine.
//It's simple, efficient, reversible, and gives decent results, but it has no toe and the shoulders quickly turn white.
//
//Note that I removed the distinction between tone-mapped RGB and tone-mapped Luma.
//Philosophically, I agree with the idea of ​​trying to remap brightness to displayable values ​​while preserving hue.
//But you'll run into the problem of one or more color channels ending up brighter than 1.0 and getting clipped.

float3 ToneMap(float3 hdr)
{
    return 1 - exp2(-hdr);
}

float3 InverseToneMap(float3 sdr)
{
    return -log2(max(1e-6, 1 - sdr));
}

float ToneMapLuma(float luma)
{
    return 1 - exp2(-luma);
}

float InverseToneMapLuma(float luma)
{
    return -log2(max(1e-6, 1 - luma));
}

//
// ACES
//

//Next Generation Movies tone operators.

float3 ToneMapACES(float3 hdr)
{
    const float A = 2.51, B = 0.03, C = 2.43, D = 0.59, E = 0.14;
    return saturate((hdr * (A * hdr + B)) / (hdr * (C * hdr + D) + E));
}

float3 InverseToneMapACES(float3 sdr)
{
    const float A = 2.51, B = 0.03, C = 2.43, D = 0.59, E = 0.14;
    return 0.5 * (D * sdr - sqrt(((D * D - 4 * C * E) * sdr + 4 * A * E - 2 * B * D) * sdr + B * B) - B) / (A - C * sdr);
}

#endif