#pragma once
#include "RenderObject.h"
#include "EngineUtility.h"
#include "Camera.h"
#include "TextureManager.h"
#include "ModelMesh.h"


struct TerrainInitInfo
{
	string HeightMapFilename;
	string LayerMapFilename[5];
	string BlendMapFilename;
	UINT TerrainTitleSize;
	UINT TerrainEachTitleSize;
	float HeightScale;
	UINT HeightmapWidth;
	UINT HeightmapHeight;
	float CellSpacing;
};

class Terrain :
    public RenderObject
{
public:
	Terrain();
	~Terrain();

	void Update(float dt);

	float GetWidth()const;
	float GetDepth()const;
	float GetHeight(float x, float z)const;
private:
	void CreateMaterials();
	void LoadHeightMapFromFile(string filename);

	void Smooth();
	bool InBounds(int i, int j);
	float Average(int i, int j);
	void CalcAllPatchBoundsY();
	void CalcPatchBoundsY(UINT i, UINT j);
	void BuildQuadPatchGeometry();
private:
	//PSO
	static GraphicsPSO mPSO;

	CD3DX12_CPU_DESCRIPTOR_HANDLE m_HeightMapDescriptor;
	TerrainInitInfo mTerrainInfo;


	UINT mNumPatchVertices = 0;
	UINT mNumPatchQuadFaces = 0;

	UINT mNumPatchVertRows = 0;
	UINT mNumPatchVertCols = 0;

	std::vector<XMFLOAT4X4> mWorld;

	std::vector<XMFLOAT2> mPatchBoundsY;
	std::vector<float> mHeightmap;

	std::unique_ptr<MeshGeometry> mGeometries;

	//srv index
	int mBlendMapIndex;
	int mHeightMapIndex;
};

