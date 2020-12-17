#include "Common.hlsli"



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


struct GSOutput
{
	float4 pos : SV_POSITION;
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float3 TangentW:TANGENT;
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
		if (input[0].PosW.y >= -1.0f /*&& input[0].PosW.y <= 20.0f*/)
		{

			float time = gTotalTime / 20.0f;


			//float3 randomRGB = gRandomTex.SampleLevel(samRandom, UVoffset.x / UVoffset.y, 0).rgb;// sin(frac(PosW.x) + frac(PosW.z));
			//float random = sin(randomRGB.r + randomRGB.b + randomRGB.g);
			float random = sin(time);
			float3 randomRGB = float3(sin(input[0].PosW.x), cos(input[0].PosW.y), sin(input[0].PosW.z));
			//风的影响系数
			float windCoEff = 0.0f;
			float2 wind = float2(sin(randomRGB.r * time * 15), sin(randomRGB.g * time * 15));
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
			float3 up = cross(look, right);


			// Compute triangle strip vertices (quad) in world space.
			float HalfWidth = 0.8f;
			float PerHeight = HalfWidth * 3.0f;

			float4 v[12];
			float3 Height = PerHeight * up;
			float3 Width = HalfWidth * right;


			v[0] = float4(input[0].PosW - Width + Height * 0, 1.0f);
			v[0].xz += wind.xy * windCoEff;
			v[0].y -= windForce * windCoEff * 0.8;
			v[1] = float4(input[0].PosW + Width + Height * 0, 1.0f);
			v[1].xz += wind.xy * windCoEff;
			v[1].y -= windForce * windCoEff * 0.8;
			windCoEff += 1 - gGrassTexC[0].y;

			v[2] = float4(input[0].PosW - Width + Height * 1, 1.0f);
			v[2].xz += wind.xy * windCoEff;
			v[2].y -= windForce * windCoEff * 0.8;
			v[3] = float4(input[0].PosW + Width + Height * 1, 1.0f);
			v[3].xz += wind.xy * windCoEff;
			v[3].y -= windForce * windCoEff * 0.8;
			windCoEff += 1 - gGrassTexC[2].y;

			v[4] = float4(input[0].PosW - Width + Height * 2, 1.0f);
			v[4].xz += wind.xy * windCoEff;
			v[4].y -= windForce * windCoEff * 0.8;
			v[5] = float4(input[0].PosW + Width + Height * 2, 1.0f);
			v[5].xz += wind.xy * windCoEff;
			v[5].y -= windForce * windCoEff * 0.8;
			windCoEff += 1 - gGrassTexC[4].y;

			v[6] = float4(input[0].PosW - Width + Height * 3, 1.0f);
			v[6].xz += wind.xy * windCoEff;
			v[6].y -= windForce * windCoEff * 0.8;
			v[7] = float4(input[0].PosW + Width + Height * 3, 1.0f);
			v[7].xz += wind.xy * windCoEff;
			v[7].y -= windForce * windCoEff * 0.8;
			windCoEff += 1 - gGrassTexC[6].y;

			v[8] = float4(input[0].PosW - Width + Height * 4, 1.0f);
			v[8].xz += wind.xy * windCoEff;
			v[8].y -= windForce * windCoEff * 0.8;
			v[9] = float4(input[0].PosW + Width + Height * 4, 1.0f);
			v[9].xz += wind.xy * windCoEff;
			v[9].y -= windForce * windCoEff * 0.8;
			windCoEff += 1 - gGrassTexC[8].y;

			v[10] = float4(input[0].PosW - Width + Height * 5, 1.0f);
			v[10].xz += wind.xy * windCoEff;
			v[10].y -= windForce * windCoEff * 0.8;
			v[11] = float4(input[0].PosW + Width + Height * 5, 1.0f);
			v[11].xz += wind.xy * windCoEff;
			v[11].y -= windForce * windCoEff * 0.8;
			windCoEff += 1 - gGrassTexC[10].y;


			GSOutput element;
			[unroll]
			for (int J = 0; J < 12; ++J)
			{
				// Project to homogeneous clip space.
				element.pos = mul(v[J], gViewProj);
				element.PosW = v[J].xyz;
				element.NormalW = look;
				element.TangentW = right;
				element.Tex = gGrassTexC[J];
				output.Append(element);
			}
		}
}