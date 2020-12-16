#include "Common.hlsli"
#include "TerrainConstBuffer.hlsli"
struct DomainOut
{
	float3 PosW     : POSITION;
};

struct PatchTess
{
	float EdgeTess[4]   : SV_TessFactor;
	float InsideTess[2] : SV_InsideTessFactor;
};

struct HullOut
{
	float3 PosW     : POSITION;
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

	float c = (dout.PosW.x + 0.5f * 2048.0f);
	float d = -(dout.PosW.z - 0.5f * 2048.0f);
	float2 UV = float2(c / 2048.0f, d / 2048.0f);

	// Displacement mapping
	dout.PosW.y = gSRVMap[mHeightMapIndex].SampleLevel(gsamLinearWrap, UV, 0).r;
	// Project to homogeneous clip space.
	//dout.PosH = mul(float4(dout.PosW, 1.0f), gViewProj);

	return dout;
}