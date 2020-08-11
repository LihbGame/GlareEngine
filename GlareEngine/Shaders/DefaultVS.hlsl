// Include structures and functions for lighting.
#include "Common.hlsli"



struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
    float3 TangentL: TANGENT;
    float2 TexC    : TEXCOORD;
};

struct VertexOut
{
    float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW:TANGENT;
    float2 TexC    : TEXCOORD; 
};



float3 CalculateWavesDisplacement(float3 vert)
{

    float4 _A = float4(10, 10, 13, 0);//A_振幅,从水平面到波峰的高度
    float4 _Q = float4(0.46, 0, 0, 0);//Q_用来控制波的陡度，其值越大，则波越陡
    float4 _S = float4(100, 100, 0, 0);//S_速度,每秒种波峰移动的距离
    float4 _Dx = float4(-0.08, 0, 0, 0); //X_垂直于波峰沿波前进方向的水平矢量X
    float4 _Dz = float4(-0.4, 0, 0, 0);//Z_垂直于波峰沿波前进方向的水平矢量Z
    float4 _L = float4(300, 200, 100, 0);//L_波长,世界空间中波峰到波峰之间的距离

    float4 _A1 = float4(20, 24,26, 0);//A_振幅,从水平面到波峰的高度
    float4 _Q1 = float4(0.46, 0,0 , 0);//Q_用来控制波的陡度，其值越大，则波越陡
    float4 _S1 = float4(100, 50, 0, 0);//S_速度,每秒种波峰移动的距离
    float4 _Dx1 = float4(-1.06, 0, 0, 0); //X_垂直于波峰沿波前进方向的水平矢量X
    float4 _Dz1 = float4(-1.02, 0, 0, 0);//Z_垂直于波峰沿波前进方向的水平矢量Z
    float4 _L1 = float4(500,450,230, 2);//L_波长,世界空间中波峰到波峰之间的距离

    float4 _A2 = float4(15, 15, 16, 0);//A_振幅,从水平面到波峰的高度
    float4 _Q2= float4(0.46, 0, 0, 0);//Q_用来控制波的陡度，其值越大，则波越陡
    float4 _S2 = float4(40, 40, 0, 0);//S_速度,每秒种波峰移动的距离
    float4 _Dx2 = float4(-1.4, 0, 0, 0); //X_垂直于波峰沿波前进方向的水平矢量X
    float4 _Dz2= float4(0.9, 0, 0, 0);//Z_垂直于波峰沿波前进方向的水平矢量Z
    float4 _L2 = float4(350,300, 440, 2);//L_波长,世界空间中波峰到波峰之间的距离
    

    float4 _A3 = float4(10, 4, 5, 0);//A_振幅,从水平面到波峰的高度
    float4 _Q3 = float4(0.66, 0, 0, 0);//Q_用来控制波的陡度，其值越大，则波越陡
    float4 _S3 = float4(10, 10, 0, 0);//S_速度,每秒种波峰移动的距离
    float4 _Dx3 = float4(-1.04, 0, 0, 0); //X_垂直于波峰沿波前进方向的水平矢量X
    float4 _Dz3 = float4(-0.09, 0, 0, 0);//Z_垂直于波峰沿波前进方向的水平矢量Z
    float4 _L3 = float4(100, 100, 100, 2);//L_波长,世界空间中波峰到波峰之间的距离
    //float PI = 3.141592f;
    float4 sinp = float4(0, 0, 0, 0);
    float4 cosp = float4(0, 0, 0, 0);

    float3 Gpos = float3(0, 0, 0);
    float3 Gpos1 = float3(0, 0, 0);
    float3 Gpos2 = float3(0, 0, 0);
    float3 Gpos3 = float3(0, 0, 0);
    float4 w = 2 * PI / _L;
    float4 psi = _S * 2 * PI / _L;
    float4 phase = w * _Dx * vert.x + w * _Dz * vert.z + psi * gTotalTime;
    sincos(phase, sinp, cosp);
    Gpos.x = dot(_Q * _A * _Dx, cosp);
    Gpos.z = dot(_Q * _A * _Dz, cosp);
    Gpos.y = dot(_A, sinp);

    float4 w1 = 2 * PI / _L1;
    float4 psi1 = _S1 * 2 * PI / _L1;
    float4 phase1 = w1 * _Dx1 * vert.x + w1 * _Dz1 * vert.z + psi1 * gTotalTime;
    sincos(phase1, sinp, cosp);
    Gpos1.x = dot(_Q1 * _A1 * _Dx1, cosp);
    Gpos1.z = dot(_Q1 * _A1 * _Dz1, cosp);
    Gpos1.y = dot(_A1, sinp);

    float4 w2 = 2 * PI / _L2;
    float4 psi2 = _S2 * 2 * PI / _L2;
    float4 phase2 = w2 * _Dx2 * vert.x + w2 * _Dz2 * vert.z + psi2 * gTotalTime;
    sincos(phase2, sinp, cosp);
    Gpos2.x = dot(_Q2 * _A2 * _Dx2, cosp);
    Gpos2.z = dot(_Q2 * _A2 * _Dz2, cosp);
    Gpos2.y = dot(_A2, sinp);

    float4 w3 = 2 * PI / _L3;
    float4 psi3 = _S3 * 2 * PI / _L3;
    float4 phase3 = w3 * _Dx3 * vert.x + w3 * _Dz3 * vert.z + psi3 * gTotalTime;
    sincos(phase3, sinp, cosp);
    Gpos3.x = dot(_Q3 * _A3 * _Dx3, cosp);
    Gpos3.z = dot(_Q3 * _A3 * _Dz3, cosp);
    Gpos3.y = dot(_A3, sinp);
    return float3(Gpos.x+ Gpos1.x+Gpos2.x+ Gpos3.x, Gpos.y + Gpos1.y + Gpos2.y + Gpos3.y, Gpos.z + Gpos1.z+ Gpos2.z + Gpos3.z);
}

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;
	

    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);

    //posW.xyz += CalculateWavesDisplacement(posW.xyz);
    vout.PosW = posW.xyz;

    //PS NEED
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
    vout.TangentW = mul(vin.TangentL, (float3x3)gWorld);
    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);

    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;
    return vout;
}
