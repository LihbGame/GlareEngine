#include "Common.hlsli"

struct VertexIn
{
    float3 Pos       : POSITION;
    float3 Normal    : NORMAL;
    float3 Tangent   : TANGENT;
    float2 TexCoord0 : TEXCOORD;
};

struct VertexOut
{
    float4 PosH       : SV_POSITION;
    float3 Eye        : TEXCOORD0;
    float4 Wave0      : TEXCOORD1;
    float2 Wave1      : TEXCOORD2;
    float2 Wave2      : TEXCOORD3;
    float2 Wave3      : TEXCOORD4;
    float4 ScreenPos  : TEXCOORD5;
    float2 HeighTex   : TEXCOORD6;
    float3 PosW       : TEXCOORD7;
    float time        : Time;
};


VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    vout.PosW = mul(float4(vin.Pos, 1.0f), gWorld).rgb;

    // Transform to homogeneous clip space.
    vout.PosH = mul(float4(vout.PosW, 1.0f), gViewProj);
    float tans = gTotalTime * 0.005;
    float2 fTranslation = float2(tans, tans);
    float2 vTexCoords = vin.Pos.xz * 0.01;

    // Scale texture coordinates to get mix of low/high frequency details
    vout.Wave0.xy = vTexCoords.xy + fTranslation * 2.0;
    vout.Wave1.xy = vTexCoords.xy * 2.0 + fTranslation * 4.0;
    vout.Wave2.xy = vTexCoords.xy * 4.0 + fTranslation * 2.0;
    vout.Wave3.xy = vTexCoords.xy * 8.0 + fTranslation;
    //Heigh Tex
    vout.HeighTex = vin.TexCoord0;

    // perspective corrected projection
    float4 vHPos = vout.PosH;
    vout.Wave0.zw = vHPos.w;

    vHPos.y = -vHPos.y;
    vout.ScreenPos.xy = (vHPos.xy + vHPos.w) * 0.5;
    vout.ScreenPos.zw = float2(1, vHPos.w);

    vout.time = abs(cos(gTotalTime / 3));
    vout.Eye = gEyePosW.xyz -vout.PosW;
    return vout;
}