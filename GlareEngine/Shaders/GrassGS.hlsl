#include "Common.hlsli"
#include "TerrainConstBuffer.hlsli"

#define halfWidth 0.8f
#define perHeight 2.4f
#define halfheight 6.0f
static	float2 gGrassTexC[12] =
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

	float2(0.0f,0.003f),
	float2(1.0f,0.003f)
};


static float4 v[12] = {
float4(0.0f,0.0f,0.0f,0.0f),
float4(0.0f,0.0f,0.0f,0.0f),

float4(0.0f,0.0f,0.0f,0.0f),
float4(0.0f,0.0f,0.0f,0.0f),

float4(0.0f,0.0f,0.0f,0.0f),
float4(0.0f,0.0f,0.0f,0.0f),

float4(0.0f,0.0f,0.0f,0.0f),
float4(0.0f,0.0f,0.0f,0.0f),

float4(0.0f,0.0f,0.0f,0.0f),
float4(0.0f,0.0f,0.0f,0.0f),

float4(0.0f,0.0f,0.0f,0.0f),
float4(0.0f,0.0f,0.0f,0.0f)
};



struct GSOutput
{
	float4 pos : SV_POSITION;
	float3 PosW    : POSITION;
	//float3 NormalW : NORMAL;
	//float3 TangentW:TANGENT;
	float2 Tex  : TEXCOORD;
};


struct VertexOut
{
	float3 PosW    : POSITION;
};




// The draw GS just expands points into Grass.

	// v10--v11
	//   |\ |
	//   | \|
	//  v8--v9
	//   |\ |
	//   | \|
	//  v6--v7
	//   |\ |
	//   | \|
	//  v4--v5
	//   |\ |
	//   | \|
	//  v2--v3
	//   |\ |
	//   | \|
	//  v0--v1
[maxvertexcount(12)]
void GS(
	point VertexOut input[1],
	inout  TriangleStream<GSOutput> output)
{
	bool isGrassClip;
	float3 tempCenter = input[0].PosW;
	tempCenter.y +=halfheight;
	if (isReflection)
	{
		tempCenter.y = -tempCenter.y;
		isGrassClip = AABBOutsideFrustumTest(tempCenter, float3(halfWidth, halfWidth, halfheight), gWorldFrustumPlanes);
	}
	else
	{
		isGrassClip = AABBOutsideFrustumTest(tempCenter, float3(halfWidth, halfWidth, halfheight), gWorldFrustumPlanes);
	}


	if (input[0].PosW.y >= 0.0f /*&& input[0].PosW.y <= 20.0f*/
		&& !isGrassClip)
	{

		float time = gTotalTime*2;
		float3 randomRGB = gSRVMap[mRGBNoiseMapIndex].SampleLevel(gsamLinearWrap, float2(input[0].PosW.x, input[0].PosW.z), 0).rgb;

		float random = sin(randomRGB.r + randomRGB.b + randomRGB.g);
		//风的影响系数
		float windCoEff = 0.0f;
		float2 wind = float2(sin(randomRGB.r * time), sin(randomRGB.g * time));
		wind.x += (sin(time + input[0].PosW.x / 25) + sin((time + input[0].PosW.x / 15) + 50)) * 0.5;
		wind.y += cos(time + input[0].PosW.z / 80);
		wind *= lerp(0.7, 1.0, 1.0 - random);

		float oscillationStrength = 2.5f;
		float sinSkewCoeff = random;
		float lerpCoeff = (sin(oscillationStrength * time + sinSkewCoeff) + 1.0) / 2;
		float2 leftWindBound = wind * (1.0 - 0.05);
		float2 rightWindBound = wind * (1.0 + 0.05);

		wind = lerp(leftWindBound, rightWindBound, lerpCoeff);

		float randomAngle = lerp(-3.14, 3.14, random);
		float randomMagnitude = lerp(0, 1., random);
		float2 randomWindDir = float2(sin(randomAngle), cos(randomAngle));
		wind += randomWindDir * randomMagnitude;

		float windForce = length(wind);



		float3 look = normalize(gEyePosW - input[0].PosW.rgb);
		look = normalize(float3(look.x, 0.0f, look.z));
		float3 right = normalize(cross(float3(0, 1, 0), look));
		float3 up = float3(0, 1, 0);//cross(look, right);


		// Compute triangle strip vertices (quad) in world space.
		float HalfWidth = halfWidth;
		float PerHeight = perHeight;

		float3 Height = PerHeight * up;
		float3 Width = HalfWidth * right;

		[unroll]
		for (int i = 0; i < 6; ++i)
		{
			int index = i * 2;
			v[index] = float4(input[0].PosW - Width + Height * i, 1.0f);
			v[index].xz += wind.xy * windCoEff;
			v[index].y -= windForce * windCoEff * 0.8;
			v[index +1] = float4(input[0].PosW + Width + Height * i, 1.0f);
			v[index +1].xz += wind.xy * windCoEff;
			v[index +1].y -= windForce * windCoEff * 0.8;
			windCoEff += 1 - gGrassTexC[index].y;
		}


		GSOutput element;
		[unroll]
		for (int J = 0; J < 12; ++J)
		{
			if (isReflection)
			{
				v[J].y *= -1.0f;
			}

			// Project to homogeneous clip space.
			element.pos = mul(v[J], gViewProj);
			element.PosW = v[J].xyz;
			//element.NormalW = look;
			//element.TangentW = right;
			element.Tex = gGrassTexC[J];
			output.Append(element);
		}
	}
}