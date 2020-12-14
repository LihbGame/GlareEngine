#pragma once
#include "L3DUtil.h"

class Grass
{
public:
	Grass(ID3D12Device* device, float GrassWidth, float GrassDepth, int VertRows, int VertCols, std::vector<XMFLOAT2>& mPatchBoundsY);
	~Grass();
	void BuildGrassVB();
private:
	ID3D11ShaderResourceView* mGrassTexSRV;
	ID3D11ShaderResourceView* mGrassBlendTexSRV;
	ID3D11ShaderResourceView* mRandomSRV;

	ID3D12Device* mDevice;

	std::vector<XMFLOAT2> mPatchBoundsY;
	int mNumVertRows;
	int mNumVertCols;
	float mGrassWidth;
	float mGrassDepth;
};

