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
	int mRGBNoiseMapIndex;
	float gMinWind;
	float gMaxWind;
};
