#pragma once
#include "L3DUtil.h"
class L3DTextureManage;
class Grass
{
public:
	Grass(ID3D12Device* device,
		ID3D12GraphicsCommandList* CommandList,
		L3DTextureManage* TextureManage,
		float GrassWidth, float GrassDepth, 
		int VertRows, int VertCols);
	~Grass();
	void BuildGrassVB();

	MeshGeometry* GetMeshGeometry()const { return mGeometries.get(); };

	void BuildMaterials();

	void FillSRVDescriptorHeap(int* SRVIndex,
		CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor);

	int GetRGBNoiseMapIndex() { return mRGBNoiseMapIndex; }
private:
	ID3D11ShaderResourceView* mGrassTexSRV;
	ID3D11ShaderResourceView* mGrassBlendTexSRV;
	ID3D11ShaderResourceView* mRandomSRV;

	ID3D12Device* mDevice;

	UINT mNumVertRows;
	UINT mNumVertCols;
	float mGrassWidth;
	float mGrassDepth;

	ID3D12GraphicsCommandList* mCommandList;
	L3DTextureManage* mTextureManage;

	std::unique_ptr<MeshGeometry> mGeometries;

	//srv index
	int mRGBNoiseMapIndex;
};

