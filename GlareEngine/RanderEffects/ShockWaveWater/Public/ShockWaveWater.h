#pragma once
#include "L3DUtil.h"
class ShockWaveWater
{
public:
	ShockWaveWater(ID3D12Device* device, UINT width, UINT height, bool IsMsaa);
	~ShockWaveWater();

	ID3D12Resource* ReflectionSRV()const;
	ID3D12Resource* SingleReflectionSRV()const;
	ID3D12Resource* SingleRefractionSRV()const;
	ID3D12Resource* FoamSRV()const;

	ID3D12Resource* ReflectionRTV()const;
	ID3D12Resource* ReflectionDepthMapDSV()const;
	D3D12_CPU_DESCRIPTOR_HANDLE ReflectionDescriptor()const;

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE RefractionSRVDescriptor,
		CD3DX12_CPU_DESCRIPTOR_HANDLE ReflectionRTVDescriptor);
private:
	void BuildDescriptors();
	void BuildResource();
private:
	ShockWaveWater(const ShockWaveWater& rhs);
	ShockWaveWater& operator=(const ShockWaveWater& rhs);
private:
	ID3D12Device* md3dDevice = nullptr;

	UINT mWidth;
	UINT mHeight;
	bool mIs4xMsaa;
	Microsoft::WRL::ComPtr<ID3D12Resource> mReflectionSRV;
	Microsoft::WRL::ComPtr<ID3D12Resource> mFoamSRV;
	Microsoft::WRL::ComPtr<ID3D12Resource> mSingleReflectionSRV;
	Microsoft::WRL::ComPtr<ID3D12Resource> mSingleRefractionSRV;

	Microsoft::WRL::ComPtr<ID3D12Resource> mReflectionRTV;
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthMapDSV;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mReflectionRTVHeap;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mRefractionSRVDescriptor;
};

