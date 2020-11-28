#pragma once
#include "L3DUtil.h"
#include "L3DCamera.h"
#include "L3DTextureManage.h"
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
	HeightmapTerrain(ID3D12Device* device, ID3D12GraphicsCommandList* dc, InitInfo& initInfo, ID3D12Resource* RandomTexSRV);
	~HeightmapTerrain();


	float GetWidth()const;
	float GetDepth()const;
	float GetHeight(float x, float z)const;

	XMMATRIX GetWorld()const;
	void SetWorld(CXMMATRIX M);

	void Init();

	void Draw(ID3D11DeviceContext* dc, const Camera& cam, bool isReflection);
	void Update(float dt);

	ID3D12Resource* GetHeightMapSRV() { return mHeightMapSRV.Get(); }

private:
	void LoadHeightmapAsset();
	void Smooth();
	bool InBounds(int i, int j);
	float Average(int i, int j);
	void CalcAllPatchBoundsY();
	void CalcPatchBoundsY(UINT i, UINT j);
	void BuildQuadPatchVB(ID3D12Device* device);
	void BuildQuadPatchIB(ID3D12Device* device);
	void BuildHeightmapSRV(ID3D12Device* device);

private:

	L3DTextureManage* pTextureManage = nullptr;

	// Divide height map into patches such that each patch has CellsPerPatch cells
	// and CellsPerPatch+1 vertices.  Use 64 so that if we tessellate all the way 
	// to 64, we use all the data from the height map.  
	static const int CellsPerPatch = 64;

	ID3D11Buffer* mQuadPatchVB = nullptr;
	ID3D11Buffer* mQuadPatchIB = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> mLayerMapArraySRV;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBlendMapSRV;
	Microsoft::WRL::ComPtr<ID3D12Resource> mHeightMapSRV;
	Microsoft::WRL::ComPtr<ID3D12Resource> mGrassMapSRV;

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

	//grass
	//Grass mGrass;
};
