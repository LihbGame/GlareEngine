#define PacthSize 32*32

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


	float4 gOffsetPosition[PacthSize];
	float4 gBoundY[PacthSize];

	int mHeightMapIndex;
	int mBlendMapIndex;
	int pad1;
	int pad2;
};
