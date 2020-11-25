#pragma once
#include "L3DUtil.h"
#include "L3DCamera.h"
class HeightmapTerrain
{
public:
	struct InitInfo
	{
		std::wstring HeightMapFilename;
		std::wstring LayerMapFilename0;
		std::wstring LayerMapFilename1;
		std::wstring LayerMapFilename2;
		std::wstring LayerMapFilename3;
		std::wstring LayerMapFilename4;
		std::wstring BlendMapFilename;
		float HeightScale;
		UINT HeightmapWidth;
		UINT HeightmapHeight;
		float CellSpacing;
	};

public:
	HeightmapTerrain();
	~HeightmapTerrain();


	float GetWidth()const;
	float GetDepth()const;
	float GetHeight(float x, float z)const;

	XMMATRIX GetWorld()const;
	void SetWorld(CXMMATRIX M);

	void Init(ID3D11Device* device, ID3D11DeviceContext* dc, const InitInfo& initInfo, ID3D11ShaderResourceView* RandomTexSRV);

	void Draw(ID3D11DeviceContext* dc, const Camera& cam,bool isReflection);
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

	// Divide heightmap into patches such that each patch has CellsPerPatch cells
	// and CellsPerPatch+1 vertices.  Use 64 so that if we tessellate all the way 
	// to 64, we use all the data from the heightmap.  
	static const int CellsPerPatch = 64;

	ID3D11Buffer* mQuadPatchVB;
	ID3D11Buffer* mQuadPatchIB;

	Microsoft::WRL::ComPtr<ID3D12Resource> mLayerMapArraySRV;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBlendMapSRV;
	Microsoft::WRL::ComPtr<ID3D12Resource> mHeightMapSRV;
	Microsoft::WRL::ComPtr<ID3D12Resource> mGrassMapSRV;

	InitInfo mInfo;

	UINT mNumPatchVertices;
	UINT mNumPatchQuadFaces;

	UINT mNumPatchVertRows;
	UINT mNumPatchVertCols;

	XMFLOAT4X4 mWorld;

	Material mMat;

	std::vector<XMFLOAT2> mPatchBoundsY;
	std::vector<float> mHeightmap;

	//grass
	//Grass mGrass;
};
