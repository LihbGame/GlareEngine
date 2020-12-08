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
	float2 Tex      : TEXCOORD0;
	float2 BoundsY  : TEXCOORD1;
};




