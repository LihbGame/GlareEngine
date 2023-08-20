#include "../../Misc/CommonResource.hlsli"
#include "../../Misc/ShaderUtility.hlsli"


SamplerState LinearClamp : register(s0);
Texture2D<float3> SourceTexture : register(t0);
StructuredBuffer<float> Exposure : register(t1);

RWTexture2D<float3> BloomResult : register(u0);
RWTexture2D<uint> LumaResult : register(u1);

cbuffer BloomConstBuffer: register(b0)
{
	float2 g_inverseOutputSize;
	float g_bloomThreshold;
}



[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	//The four samples will be in the middle of the quadrant they cover.
	float2 uv = (DTid.xy + 0.5) * g_inverseOutputSize;
	float3 color = SourceTexture.SampleLevel(LinearClamp, uv, 0);
	float Luminance = RGBToLuminance(color);
	color *= max(EPS, Luminance - g_bloomThreshold * Exposure[1]) / (Luminance + EPS);
	BloomResult[DTid.xy] = color;


	if (Luminance == 0.0)
	{
		LumaResult[DTid.xy] = 0;
	}
	else
	{
		const float MinLog = Exposure[2];
		const float RcpLogRange = Exposure[5];
		float logLuma = saturate((log2(Luminance) - MinLog) * RcpLogRange);		// remap to [0.0, 1.0]
		LumaResult[DTid.xy] = logLuma * 254.0 + 1.0;							// remap to [1, 255]
	}

}