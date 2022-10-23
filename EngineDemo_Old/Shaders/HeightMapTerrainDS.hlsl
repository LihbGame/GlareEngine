#include "TerrainConstBuffer.hlsli"
#include "Fog.hlsli"

#define gTexScale float2(50.0f,50.0f)

struct DomainOut
{
	float4 PosH     : SV_POSITION;
	float3 PosW     : POSITION0;
	float4 ShadowPosH : POSITION1;
	float2 Tex      : TEXCOORD0;
	float2 TiledTex : TEXCOORD1;
    float FogFactor : TEXCOORD2;
	float ClipValue : SV_ClipDistance0;//�ü�ֵ�ؼ���
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

// ϸ����������ÿ�����㶼���������ɫ���� ����ϸ�ֺ�Ķ�����ɫ��һ����
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
    float height = gSRVMap[mHeightMapIndex].SampleLevel(gsamLinearClamp, dout.Tex, 0).r;
	
    //dout.PosW += gSRVMap[mRGBNoiseMapIndex].SampleLevel(gsamLinearWrap, dout.Tex, 0).rgb * 200;
	//������ռ䣬���ڳ��Բü���С����Ľ��вü����ü������������ļ����岿��
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

	// NOTE:���ǳ���ʹ�����޲����������ɫ���еķ��ߣ�
	//��������������ϵط����ƶ������������ŷ��ߵı仯���������Ե�����Ч����
	//��ˣ����ǽ���������������ɫ����

	// Project to homogeneous clip space.
	dout.PosH = mul(float4(dout.PosW, 1.0f), gViewProj);
	//fog
    dout.FogFactor = LayeredFog(100.0f, 1000.0f,10.0f, dout.PosW);
	//ExponentialFog(0.5, length(gEyePosW - dout.PosW));
	return dout;
}
