// Gamma Ramp and Code Transfer Function
//
//sRGB is both a color space (defined by three basis vectors and a white point) and a gamma ramp.
//The gamma ramp is designed to reduce the perceived error when quantizing floating point numbers to integers with a finite number of bits.
//Dark colors need more variation because our eyes are more sensitive in the dark.
//What the curve does is that it spreads dark values ​​into more code words, allowing more variation.
//Similarly, bright values ​​are combined into fewer code words, allowing less variation.
//
//The sRGB curve is not a true gamma ramp, but a piecewise function consisting of a linear part and a power function.
//When sRGB encoded colors are passed to an LCD display, they look correct on the screen,
//because the display expects colors to be encoded in sRGB, and it removes the sRGB curve to make the values ​​linear.
//When textures are encoded in sRGB, the sRGB curve needs to be removed.
#ifndef COLOR_SPACE_UTILITY
#define COLOR_SPACE_UTILITY

float3 ApplySRGBCurve(float3 x)
{
    //Approximately pow(x, 1.0 / 2.2)
    return x < 0.0031308 ? 12.92 * x : 1.055 * pow(x, 1.0 / 2.4) - 0.055;
}

float3 RemoveSRGBCurve(float3 x)
{
    //Approximately pow(x, 2.2)
    return x < 0.04045 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

// These functions avoid pow() to efficiently approximate sRGB with an error < 0.4%.
float3 ApplySRGBCurve_Fast(float3 x)
{
    return x < 0.0031308 ? 12.92 * x : 1.13005 * sqrt(x - 0.00228) - 0.13448 * x + 0.005719;
}

float3 RemoveSRGBCurve_Fast(float3 x)
{
    return x < 0.04045 ? x / 12.92 : -7.43605 * x - 31.24297 * sqrt(-0.53792 * x + 1.279924) + 35.34864;
}

//OETF recommends for content displayed on HDTV. This "gamma ramp" increases contrast appropriately for viewing in dark environments.
//Always use this curve with Limited RGB as it is used with HDTV.
float3 ApplyREC709Curve(float3 x)
{
    return x < 0.0181 ? 4.5 * x : 1.0993 * pow(x, 0.45) - 0.0993;
}

float3 RemoveREC709Curve(float3 x)
{
    return x < 0.08145 ? x / 4.5 : pow((x + 0.0993) / 1.0993, 1.0 / 0.45);
}

//This is the new HDR transfer function, also known as "PQ" for perceptual quantizer.
//Note that REC2084 does not refer to a color space either. REC2084 is usually used with the REC2020 color space.
float3 ApplyREC2084Curve(float3 L)
{
    float m1 = 2610.0 / 4096.0 / 4;
    float m2 = 2523.0 / 4096.0 * 128;
    float c1 = 3424.0 / 4096.0;
    float c2 = 2413.0 / 4096.0 * 32;
    float c3 = 2392.0 / 4096.0 * 32;
    float3 Lp = pow(L, m1);
    return pow((c1 + c2 * Lp) / (1 + c3 * Lp), m2);
}

float3 RemoveREC2084Curve(float3 N)
{
    float m1 = 2610.0 / 4096.0 / 4;
    float m2 = 2523.0 / 4096.0 * 128;
    float c1 = 3424.0 / 4096.0;
    float c2 = 2413.0 / 4096.0 * 32;
    float c3 = 2392.0 / 4096.0 * 32;
    float3 Np = pow(N, 1 / m2);
    return pow(max(Np - c1, 0) / (c2 - c3 * Np), 1 / m1);
}


// Color Space Conversions
//
//These assume linear (non-gamma encoded) values. Color space conversions are changes of basis (just like in linear algebra).
//Since color spaces are defined by three vectors (basis vectors), changing spaces involves matrix-vector multiplications.
//Note that changing color spaces may cause colors to "go out of bounds" because some color spaces have larger gamuts than others.
//When converting some colors from a wide gamut to a small gamut, negative values ​​may result, which cannot be expressed in the new color space.

// Note:  Rec.709 and sRGB share the same color primaries and white point.  Their only difference
// is the transfer curve used.

float3 REC709toREC2020(float3 RGB709)
{
    static const float3x3 ConvMat =
    {
        0.627402, 0.329292, 0.043306,
        0.069095, 0.919544, 0.011360,
        0.016394, 0.088028, 0.895578
    };
    return mul(ConvMat, RGB709);
}

float3 REC2020toREC709(float3 RGB2020)
{
    static const float3x3 ConvMat =
    {
        1.660496, -0.587656, -0.072840,
        -0.124547, 1.132895, -0.008348,
        -0.018154, -0.100597, 1.118751
    };
    return mul(ConvMat, RGB2020);
}

float3 REC709toDCIP3(float3 RGB709)
{
    static const float3x3 ConvMat =
    {
        0.822458, 0.177542, 0.000000,
        0.033193, 0.966807, 0.000000,
        0.017085, 0.072410, 0.910505
    };
    return mul(ConvMat, RGB709);
}

float3 DCIP3toREC709(float3 RGB709)
{
    static const float3x3 ConvMat =
    {
        1.224947, -0.224947, 0.000000,
        -0.042056, 1.042056, 0.000000,
        -0.019641, -0.078651, 1.098291
    };
    return mul(ConvMat, RGB709);
}


// The standard 32-bit HDR color format.Each float has a 5-bit exponent and no sign bit.
uint Pack_R11G11B10_FLOAT(float3 rgb)
{
    rgb = min(rgb, asfloat(0x477C0000));
    uint r = ((f32tof16(rgb.x) + 8) >> 4) & 0x000007FF;
    uint g = ((f32tof16(rgb.y) + 8) << 7) & 0x003FF800;
    uint b = ((f32tof16(rgb.z) + 16) << 17) & 0xFFC00000;
    return r | g | b;
}

float3 Unpack_R11G11B10_FLOAT(uint rgb)
{
    float r = f16tof32((rgb << 4) & 0x7FF0);
    float g = f16tof32((rgb >> 7) & 0x7FF0);
    float b = f16tof32((rgb >> 17) & 0x7FE0);
    return float3(r, g, b);
}

#endif