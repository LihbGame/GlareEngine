#pragma once
#include "Engine/EngineUtility.h"
#include "TextureManage.h"
class ShockWaveWater
{
public:
	ShockWaveWater(ID3D12Device* device, UINT width, UINT height, bool IsMsaa, TextureManage* TextureManage);
	~ShockWaveWater();

	ID3D12Resource* ReflectionSRV()const;
	ID3D12Resource* SingleReflectionSRV()const;
	ID3D12Resource* SingleRefractionSRV()const;
	ID3D12Resource* FoamSRV()const;

	ID3D12Resource* ReflectionRTV()const;
	D3D12_CPU_DESCRIPTOR_HANDLE ReflectionDepthMapDSVDescriptor()const;
	D3D12_CPU_DESCRIPTOR_HANDLE ReflectionDescriptor()const;
	D3D12_CPU_DESCRIPTOR_HANDLE RefractionDescriptor()const;
	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE RefractionSRVDescriptor,
		CD3DX12_CPU_DESCRIPTOR_HANDLE ReflectionRTVDescriptor,
		CD3DX12_CPU_DESCRIPTOR_HANDLE WavesBumpDescriptor);

	void OnResize(UINT newWidth, UINT newHeight);

	void FillSRVDescriptorHeap(int* SRVIndex,
		CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor);


	//srv index
	int GetWaterDumpWaveIndex()const { return mWaterDumpWaveIndex; }
	int GetWaterReflectionMapIndex()const { return mWaterReflectionMapIndex; }
	int GetWaterRefractionMapIndex()const { return mWaterRefractionMapIndex; }

private:
	void BuildDescriptors();
	void BuildResource();
	void LoadTexture();
private:
	ShockWaveWater(const ShockWaveWater& rhs);
	ShockWaveWater& operator=(const ShockWaveWater& rhs);
private:
	ID3D12Device* md3dDevice = nullptr;
	TextureManage* mTextureManage = nullptr;
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
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mReflectionDSVHeap;
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE mRefractionSRVDescriptor;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mReflectionSRVDescriptor;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mWavesBumpDescriptor;

	//srv index
	int mWaterDumpWaveIndex;
	int mWaterReflectionMapIndex;
	int mWaterRefractionMapIndex;
};

