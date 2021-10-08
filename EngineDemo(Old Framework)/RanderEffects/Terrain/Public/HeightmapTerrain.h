#pragma once
#include "EngineUtility.h"
#include "Camera.h"
#include "TextureManage.h"
#include "FrameResource.h"
#include "ModelMesh.h"
class Grass;

class HeightmapTerrain
{
public:
	struct InitInfo
	{
		string HeightMapFilename;
		string LayerMapFilename[5];
		string BlendMapFilename;
		float HeightScale;
		UINT HeightmapWidth;
		UINT HeightmapHeight;
		float CellSpacing;
	};

public:
	HeightmapTerrain(ID3D12Device* device, ID3D12GraphicsCommandList* dc, TextureManage* TextureManage, InitInfo initInfo, ID3D12Resource* RandomTexSRV);
	~HeightmapTerrain();


	float GetWidth()const;
	float GetDepth()const;
	float GetHeight(float x, float z)const;

	::MeshGeometry* GetMeshGeometry()const;


	XMMATRIX GetWorld()const;
	void SetWorld(CXMMATRIX M);

	void Update(float dt);

	ID3D12Resource* GetHeightMapSRV() { return mHeightMapSRV.Get(); }

	unordered_map<string, vector<Texture*>>& GetAllTerrainTextures()
	{
		return TerrainTextures;
	}

	void BuildHeightmapSRV(CD3DX12_CPU_DESCRIPTOR_HANDLE BlendMapDescriptor,
		CD3DX12_CPU_DESCRIPTOR_HANDLE HeightMapDescriptor);

	void GetTerrainConstant(TerrainConstants& TerrainConstant);

	Grass* GetGrass() { return mGrass.get(); };

	void BuildMaterials();

	void FillSRVDescriptorHeap(int* SRVIndex,
		CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor);

	int GetHeightMapIndex()const{ return mHeightMapIndex; }
	int GetBlendMapIndex()const { return mBlendMapIndex; }
private:
	void LoadHeightmapAsset();
	void Smooth();
	bool InBounds(int i, int j);
	float Average(int i, int j);
	void CalcAllPatchBoundsY();
	void CalcPatchBoundsY(UINT i, UINT j);
	void BuildQuadPatchGeometry();
private:

	TextureManage* pTextureManage = nullptr;

	ID3D12Device* mDevice = nullptr;
	ID3D12GraphicsCommandList* mCommandList = nullptr;

	// Divide height map into patches such that each patch has CellsPerPatch cells
	// and CellsPerPatch+1 vertices.  Use 64 so that if we tessellate all the way 
	// to 64, we use all the data from the height map.  
	static const int CellsPerPatch = 64;

	Microsoft::WRL::ComPtr<ID3D12Resource> mHeightMapSRV;
	Microsoft::WRL::ComPtr<ID3D12Resource> mHeightMapUploader;

	InitInfo mInfo;

	UINT mNumPatchVertices = 0;
	UINT mNumPatchQuadFaces = 0;

	UINT mNumPatchVertRows = 0;
	UINT mNumPatchVertCols = 0;

	XMFLOAT4X4 mWorld;

	std::vector<XMFLOAT2> mPatchBoundsY;
	std::vector<float> mHeightmap;

	//All Textures
	unordered_map<string, vector<Texture*>> TerrainTextures;

	std::unique_ptr<::MeshGeometry> mGeometries;

	//grass
	std::unique_ptr<Grass> mGrass;


	//srv index
	int mBlendMapIndex;
	int mHeightMapIndex;
};
