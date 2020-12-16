#include "Common.hlsli"


cbuffer cbGrass
{
	float2 gGrassTexC[12] =
	{
		float2(0.0f,1.0f),
		float2(1.0f,1.0f),

		float2(0.0f,0.8f),
		float2(1.0f,0.8f),

		float2(0.0f,0.6f),
		float2(1.0f,0.6f),

		float2(0.0f,0.4f),
		float2(1.0f,0.4f),

		float2(0.0f,0.2f),
		float2(1.0f,0.2f),

		float2(0.0f,0.0f),
		float2(1.0f,0.0f)
	};
};

struct GSOutput
{
	float4 pos : SV_POSITION;
};


struct DomainOut
{
	float3 PosW     : POSITION;
};


[maxvertexcount(3)]
void GS(
	triangle DomainOut input[3],
	inout  PointStream< GSOutput > output
)
{
	for (int i = 0; i < 3; ++i)
	{
		if (input[i].PosW.y >= 0.0f && input[i].PosW.y <= 20.0f)
		{
			GSOutput element;
			// Project to homogeneous clip space.
			element.pos = mul(float4(input[i].PosW, 1.0f), gViewProj);
			output.Append(element);
		}
	}
}