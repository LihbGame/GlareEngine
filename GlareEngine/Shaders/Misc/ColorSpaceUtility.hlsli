// Gamma б�ºͱ��봫�ݺ���
//
//sRGB ������ɫ�ռ�(��������������һ���׵㶨��)����٤��б�¡�
//Gamma б��ּ�ڼ��ٽ�����������Ϊ��������λ��������ʱ�ĸ�֪�� 
//��ɫ��Ҫ����仯����Ϊ���ǵ��۾��ںڰ��и����С� 
//���ߵ��������ڣ�������ֵ��ɢ������Ĵ������У��Ӷ��������ı仯�� 
//ͬ����������ֵ���ϲ��ɸ��ٵĴ����֣�������ٵı仯��
//
//sRGB ���߲���������٤��б�£����������Բ��ֺ��ݺ�����ɵķֶκ����� 
//�� sRGB �������ɫ�����ݵ� LCD ��ʾ��ʱ����������Ļ�Ͽ���������ȷ�ģ�
//��Ϊ��ʾ��������ɫ�� sRGB ���룬������ɾ���� sRGB ������ʹֵ���Ի��� 
//�������� sRGB ����ʱ����Ҫɾ�� sRGB ���ߡ�
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

//OETF �Ƽ����� HDTV ����ʾ������.�á�٤��б�¡������ʵ��������ںڰ������йۿ��ĶԱȶ�.
//ʼ�ս������������� RGB ���ʹ�ã���Ϊ���� HDTV ���ʹ�á� 
float3 ApplyREC709Curve(float3 x)
{
    return x < 0.0181 ? 4.5 * x : 1.0993 * pow(x, 0.45) - 0.0993;
}

float3 RemoveREC709Curve(float3 x)
{
    return x < 0.08145 ? x / 4.5 : pow((x + 0.0993) / 1.0993, 1.0 / 0.45);
}

//�����µ� HDR ���ݺ��������ڸ�֪������Ҳ��Ϊ��PQ��.
//��ע�⣬REC2084 Ҳ����ָ��ɫ�ռ�.REC2084 ͨ���� REC2020 ɫ�ʿռ�һ��ʹ��. 
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
// ɫ�ʿռ�ת��
//
//��Щ�ٶ�����(��٤�����)ֵ.ɫ�ʿռ�ת���ǻ����ĸı�(���������Դ�����һ��).
//������ɫ�ռ������������������������壬��˸��Ŀռ��漰���������˷�.
//��ע�⣬������ɫ�ռ���ܻᵼ����ɫ��Խ�硱����ΪĳЩ��ɫ�ռ��������ɫ�ռ���и����ɫ��.
//��ĳЩ��ɫ�ӹ�ɫ��ת��ΪСɫ��ʱ�����ܻ������ֵ�������µ�ɫ�ʿռ������޷����ġ�
//
// 
//����һ����Զ���ᶪ���޷����(���ɸ�֪)����ɫ����ɫ�ܵ����������.
//����ζ��ʹ�þ����ܿ��ɫ�ʿռ�.XYZ ��ɫ�ռ������Եġ������������ɫ�ռ䣬
//�����ҵ��������и�ֵ(�ر����� X �� Z ��).Ϊ�˾����������,���Զ� X �� Z ���н�һ����ת��,ʹ����ʼ��Ϊ��ֵ.
//���ǿ���ͨ������ Y �����;������󣬴Ӷ����� X �� Z ��������� UNORM8 ��.����û�и��õ�����,�����ɫ�ռ䱻��Ϊ YUV��

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


// The standard 32-bit HDR color format.  Each float has a 5-bit exponent and no sign bit.
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