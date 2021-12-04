// Gamma 斜坡和编码传递函数
//
//sRGB 既是颜色空间(由三个基向量和一个白点定义)又是伽马斜坡。
//Gamma 斜坡旨在减少将浮点数量化为具有有限位数的整数时的感知误差。 
//深色需要更多变化，因为我们的眼睛在黑暗中更敏感。 
//曲线的作用在于，它将暗值分散到更多的代码字中，从而允许更多的变化。 
//同样，明亮的值被合并成更少的代码字，允许更少的变化。
//
//sRGB 曲线不是真正的伽马斜坡，而是由线性部分和幂函数组成的分段函数。 
//当 sRGB 编码的颜色被传递到 LCD 显示器时，它们在屏幕上看起来是正确的，
//因为显示器期望颜色用 sRGB 编码，并且它删除了 sRGB 曲线以使值线性化。 
//当纹理用 sRGB 编码时，需要删除 sRGB 曲线。
#ifndef COLOR_SPACE_UTILITY
#define COLOR_SPACE_UTILITY

float3 ApplySRGBCurve(float3 x)
{
    // Approximately pow(x, 1.0 / 2.2)
    return x < 0.0031308 ? 12.92 * x : 1.055 * pow(x, 1.0 / 2.4) - 0.055;
}

float3 RemoveSRGBCurve(float3 x)
{
    // Approximately pow(x, 2.2)
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

//OETF 推荐用于 HDTV 上显示的内容.该“伽马斜坡”可以适当地增加在黑暗环境中观看的对比度.
//始终将此曲线与有限 RGB 结合使用，因为它与 HDTV 结合使用。 
float3 ApplyREC709Curve(float3 x)
{
    return x < 0.0181 ? 4.5 * x : 1.0993 * pow(x, 0.45) - 0.0993;
}

float3 RemoveREC709Curve(float3 x)
{
    return x < 0.08145 ? x / 4.5 : pow((x + 0.0993) / 1.0993, 1.0 / 0.45);
}

//这是新的 HDR 传递函数，对于感知量化器也称为“PQ”.
//请注意，REC2084 也不是指颜色空间.REC2084 通常与 REC2020 色彩空间一起使用. 
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

//
// 色彩空间转换
//
//这些假定线性(非伽马编码)值.色彩空间转换是基础的改变(就像在线性代数中一样).
//由于颜色空间由三个向量（基向量）定义，因此更改空间涉及矩阵向量乘法.
//请注意，更改颜色空间可能会导致颜色“越界”，因为某些颜色空间比其他颜色空间具有更大的色域.
//将某些颜色从广色域转换为小色域时，可能会产生负值，这在新的色彩空间中是无法表达的。
//
// 
//构建一个永远不会丢弃无法表达(但可感知)的颜色的颜色管道将是理想的.
//这意味着使用尽可能宽的色彩空间.XYZ 颜色空间是中性的、包罗万象的颜色空间，
//但不幸的是它具有负值(特别是在 X 和 Z 中).为了纠正这个问题,可以对 X 和 Z 进行进一步的转换,使它们始终为正值.
//它们可以通过除以 Y 来降低精度需求，从而允许将 X 和 Z 打包到两个 UNORM8 中.由于没有更好的名字,这个颜色空间被称为 YUV。

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

#endif