#include "../Misc/CommonResource.hlsli"

TextureCube gCubeMap : register(t0);

float4 main(PosVSOut pin) : SV_TARGET
{
    float4 litColor = gCubeMap.Sample(gSamplerLinearWrap, pin.PosL);
    return litColor;
}