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

//这有一些很好的属性，可以改进基本的 Reinhard。 
//首先，它有一个“脚趾”——漂亮的抛物线上升，增强了黑暗中的对比度和色彩饱和度。 
//其次，它有一个长肩部，可以在高光处提供更多细节，并且需要更长的时间去饱和。 
//它是可逆的，可缩放到 HDR 显示器，并且易于控制。
//
//选择默认常数 0.25 有两个原因。 它与 Reinhard 的效果非常接近，常数为 1.0。 
//并且常数为 0.25，在 0.25 处有一个拐点，曲线与线 y=x 接触，然后开始肩部。
//
// Note:  如果您当前正在使用 ACES 并且您按 0.6 进行预缩放，那么 k=0.30 作为替代方案看起来不错，无需任何其他调整。
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

// This is the new tone operator.  它在许多方面类似于 ACES，但使用 ALU 进行评估更简单。 
//与 Reinhard-Squared 相比，它的一个优势是肩部更快地变为白色，并为图像提供更高的整体亮度和对比度。

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

//这是首先在 HemiEngine 和 MiniEngine 中使用的旧 tone operators。 
//它简单、高效、可逆，并提供了不错的结果，但它没有脚趾，而且肩部很快就会变白。
//
//请注意，我删除了色调映射 RGB 和色调映射 Luma 之间的区别。 
//从哲学上讲，我同意在保留色调的同时尝试将亮度重新映射到可显示值的想法。 
//但是您会遇到一个或多个颜色通道最终比 1.0 更亮并被剪裁的问题。

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

//下一代电影 tone operators.

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