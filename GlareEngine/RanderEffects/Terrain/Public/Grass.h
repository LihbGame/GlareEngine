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
		int VertRows, int VertCols, 
		std::vector<XMFLOAT2>& mPatchBoundsY);
	~Grass();
	void BuildGrassVB();

	std::vector<XMFLOAT4>& GetPatchBoundsY() { return mBoundsY; };

	std::vector<XMFLOAT4>& GetOffset() { return mOffset; };

	MeshGeometry* GetMeshGeometry()const { return mGeometries.get(); };
private:
	ID3D11ShaderResourceView* mGrassTexSRV;
	ID3D11ShaderResourceView* mGrassBlendTexSRV;
	ID3D11ShaderResourceView* mRandomSRV;

	ID3D12Device* mDevice;

	std::vector<XMFLOAT2> mPatchBoundsY;
	std::vector<XMFLOAT4> mBoundsY;
	std::vector<XMFLOAT4> mOffset;
	int mNumVertRows;
	int mNumVertCols;
	float mGrassWidth;
	float mGrassDepth;

	ID3D12GraphicsCommandList* mCommandList;
	L3DTextureManage* mTextureManage;

	std::unique_ptr<MeshGeometry> mGeometries;
};

