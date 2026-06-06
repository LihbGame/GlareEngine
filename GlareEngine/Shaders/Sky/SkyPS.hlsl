#include "../Misc/CommonResource.hlsli"

RWTexture2D<float2> MotionVector : register(u0);

struct SkyVSOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : POSITION0;
    float3 CurPosition : TEXCOORD0;
    float3 PrePosition : TEXCOORD1;
};

// Exposure parameter.
#define exposure 0.3f


float4 main(SkyVSOut pin) : SV_Target
{
    float4 litColor = gCubeMaps[gSkyCubeIndex].Sample(gSamplerLinearWrap, pin.PosL);

    float2 cancelJitter = gPreJitterOffset - gCurJitterOffset;
    MotionVector[pin.PosH.xy] = (((pin.PrePosition.xy / pin.PrePosition.z) - (pin.CurPosition.xy / pin.CurPosition.z)) - cancelJitter) * float2(0.5f, -0.5f);
	// Reinhard tone mapping.
	//litColor.rgb = litColor.rgb / (litColor.rgb + float3(1.0f, 1.0f, 1.0f));

	// Exposure tone mapping.
    litColor.rgb = float3(1.0f, 1.0f, 1.0f) - exp(-litColor.rgb * exposure);

    return litColor;
}
