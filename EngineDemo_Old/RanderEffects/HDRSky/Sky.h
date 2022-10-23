#pragma once
#include "Animation/ModelMesh.h"
class TextureManage;
class Sky
{
public:
	Sky(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* CommandList,
		float radius,int sliceCount,int stackCount, 
		TextureManage* TextureManage);
	~Sky();

	void BuildSkyMesh();
	std::unique_ptr<::MeshGeometry>& GetSkyMesh();

	void BuildMaterials();

	void FillSRVDescriptorHeap(int* SRVIndex,
		CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor);
private:
	std::unique_ptr<::MeshGeometry> mSkyMesh;

	ID3D12Device* md3dDevice;
	ID3D12GraphicsCommandList* mCommandList;
	TextureManage* pTextureManage = nullptr;
	float mRadius;
	int mSliceCount;
	int mStackCount;
};

