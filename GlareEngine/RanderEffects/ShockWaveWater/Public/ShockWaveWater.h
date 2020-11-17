#pragma once
#include "L3DUtil.h"
class ShockWaveWater
{
public:
	ShockWaveWater(ID3D12Device* device, UINT width, UINT height);
	~ShockWaveWater();



	ID3D12Resource* ReflectionSRV();
	ID3D12Resource* SingleReflectionSRV();
	ID3D12Resource* SingleRefractionSRV();
	ID3D12Resource* FoamSRV();

	ID3D12Resource* ReflectionRTV();
	ID3D12Resource* ReflectionDepthMapDSV();
private:
	ShockWaveWater(const ShockWaveWater& rhs);
	ShockWaveWater& operator=(const ShockWaveWater& rhs);
private:
	UINT mWidth;
	UINT mHeight;

	Microsoft::WRL::ComPtr<ID3D12Resource> mReflectionSRV;
	Microsoft::WRL::ComPtr<ID3D12Resource> mFoamSRV;
	Microsoft::WRL::ComPtr<ID3D12Resource> mSingleReflectionSRV;
	Microsoft::WRL::ComPtr<ID3D12Resource> mSingleRefractionSRV;

	Microsoft::WRL::ComPtr<ID3D12Resource> mReflectionRTV;
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthMapDSV;


};

