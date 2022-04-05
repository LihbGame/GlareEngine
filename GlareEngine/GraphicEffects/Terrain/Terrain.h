#pragma once
#include "RenderObject.h"
#include "EngineUtility.h"
#include "Camera.h"
#include "TextureManager.h"
#include "ModelMesh.h"
#include "ConstantBuffer.h"

struct TerrainInitInfo
{
	string HeightMapFilename;
	string LayerMapFilename[5];
	string BlendMapFilename;
	UINT TerrainTileSize;
	UINT TerrainCellNumInTile;
	float CellSize;
	float HeightScale;
	UINT HeightmapWidth;
	UINT HeightmapHeight;
};

class Terrain :
    public RenderObject
{
public:
	Terrain(ID3D12GraphicsCommandList* CommandList,TerrainInitInfo TerrainInfo);
	~Terrain();

	void Update(float dt);

	float GetWidth()const;
	float GetDepth()const;
	float GetHeight(float x, float z)const;
private:
	void CreateMaterials();
	void LoadHeightMapAsset();

	void Smooth();
	bool InBounds(int i, int j);
	float Average(int i, int j);
	void CalcAllPatchBoundsY();
	XMFLOAT2 CalcPatchBoundsY(UINT i, UINT j, UINT k);
	void BuildQuadPatchGeometry(ID3D12GraphicsCommandList* CommandList);
	void BuildTerrainTransform();
	void UpdateTerrainConstantBuffer();
private:
	//PSO
	static GraphicsPSO mPSO;
	ID3D12GraphicsCommandList* m_pCommandList = nullptr;
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_HeightMapDescriptor;
	TerrainInitInfo mTerrainInfo;

	TerrainConstants mTerrainConstant;

	//Patch info
	UINT mNumPatchVertices = 0;
	UINT mNumPatchQuadFaces = 0;
	UINT mNumPatchVertRows = 0;
	UINT mNumPatchVertCols = 0;

	vector<XMFLOAT4X4> mWorldTransforms;

	//all Patch Bounds
	vector<vector<XMFLOAT2>> mAllPatchBoundsY;
	//all Tile Bounds
	vector<XMFLOAT2> mTileBoundsY;
	//Height map data (only have one map for now)
	vector<float> mHeightmap;

	float mTerrainSize;
	float mTerrainTileWidth;
	UINT mTileNum;

	unique_ptr<MeshGeometry> mGeometries;

	//SRV index
	int mBlendMapIndex = 0;
	int mHeightMapIndex = 0;
};

