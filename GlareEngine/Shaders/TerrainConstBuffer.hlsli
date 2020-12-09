cbuffer TerrainCBPass : register(b2)
{
	float gMinDist;
	float gMaxDist;

	float gMinTess;
	float gMaxTess;

	float gTexelCellSpaceU;
	float gTexelCellSpaceV;
	float gWorldCellSpace;
	bool isReflection;

	float4 gWorldFrustumPlanes[6];

	int mHeightMapIndex;
	int mBlendMapIndex;
};


struct VertexIn
{
	float3 PosL     : POSITION;
	float2 Tex      : TEXCOORD;
	float2 BoundsY  : BoundY;
};

struct VertexOut
{
	float3 PosW     : POSITION;
	float2 Tex      : TEXCOORD;
	float2 BoundsY  : BoundY;
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


struct DomainOut
{
	float4 PosH     : SV_POSITION;
	float3 PosW     : POSITION;
	float2 Tex      : TEXCOORD0;
	float2 TiledTex : TEXCOORD1;
	float ClipValue : SV_ClipDistance0;//²Ã¼ôÖµ¹Ø¼ü×Ö
};
