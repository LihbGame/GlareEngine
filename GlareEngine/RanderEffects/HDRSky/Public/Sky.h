#pragma once
#include "L3DUtil.h"
class Sky
{
public:
	Sky(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* CommandList,float radius,int sliceCount,int stackCount);
	~Sky();

	void BuildSkyMesh();
	std::unique_ptr<MeshGeometry>& GetSkyMesh();

	void BuildSkyMaterials(int MatCBIndex);
	std::unique_ptr<Material>& GetSkyMaterial();
private:
	std::unique_ptr<Material> mMaterial;
	std::unique_ptr<MeshGeometry> mSkyMesh;



	ID3D12Device* md3dDevice;
	ID3D12GraphicsCommandList* mCommandList;

	
	float mRadius;
	int mSliceCount;
	int mStackCount;
};

