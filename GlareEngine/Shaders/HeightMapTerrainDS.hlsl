#include "TerrainConstBuffer.hlsli"
#include "Common.hlsli"

#define gTexScale float2(25.0f,25.0f)

struct DomainOut
{
	float4 PosH     : SV_POSITION;
	float3 PosW     : POSITION0;
	float4 ShadowPosH : POSITION1;
	float2 Tex      : TEXCOORD0;
	float2 TiledTex : TEXCOORD1;
    float FogFactor : TEXCOORD2;
	float ClipValue : SV_ClipDistance0;//裁剪值关键字
};

struct PatchTess
{
	float EdgeTess[4]   : SV_TessFactor;
	float InsideTess[2] : SV_InsideTessFactor;
};

struct HullOut
{
	float3 PosW     : POSITION;
	float2 Tex      : TEXCOORD;
};

// 细分器创建的每个顶点都会调用域着色器。 就像细分后的顶点着色器一样。
[domain("quad")]
DomainOut DS(PatchTess patchTess,
	float2 uv : SV_DomainLocation,
	const OutputPatch<HullOut, 4> quad)
{
	DomainOut dout;

	// Bilinear interpolation.
	dout.PosW = lerp(
		lerp(quad[0].PosW, quad[1].PosW, uv.x),
		lerp(quad[2].PosW, quad[3].PosW, uv.x),
		uv.y);

	dout.Tex = lerp(
		lerp(quad[0].Tex, quad[1].Tex, uv.x),
		lerp(quad[2].Tex, quad[3].Tex, uv.x),
		uv.y);

	// Tile layer textures over terrain.
	dout.TiledTex = dout.Tex * gTexScale;

	//dout.PosW = mul(float4(dout.PosW, 1.0f),gWorld).xyz;

	// Displacement mapping
	float height = gSRVMap[mHeightMapIndex].SampleLevel(gsamLinearWrap, dout.Tex, 0).r;
	
	dout.PosW+=gSRVMap[mRGBNoiseMapIndex].SampleLevel(gsamLinearWrap, dout.Tex, 0).rgb*2;
	//在世界空间，对于乘以裁剪面小于零的进行裁剪，裁剪不满足条件的几何体部分
	if (isReflection)
	{
		dout.PosW.y = -height;
		float3 ClipPlane = float3(0.0f, -1.0f, 0.0f);
		dout.ClipValue = dot(dout.PosW, ClipPlane);
	}
	else
	{
		dout.PosW.y = height;
		dout.ClipValue = 1.0f;
	}


	// Generate projective tex-coords to project shadow map onto scene.
	dout.ShadowPosH = mul(float4(dout.PosW,1.0f), gShadowTransform);

	// NOTE:我们尝试使用有限差分来计算着色器中的法线，
	//但顶点会连续不断地分数移动，甚至会随着法线的变化而产生明显的闪光效果。
	//因此，我们将计算移至像素着色器。

	// Project to homogeneous clip space.
	dout.PosH = mul(float4(dout.PosW, 1.0f), gViewProj);
	//fog
    dout.FogFactor = ExponentialFog(0.5, length(gEyePosW - dout.PosW));
	return dout;
}
