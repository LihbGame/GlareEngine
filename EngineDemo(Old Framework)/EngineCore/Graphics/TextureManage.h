#pragma once

#include "EngineUtility.h"
#include "TextureManager.h"
#include <DDS.h>

using namespace GlareEngine;

enum PBRTextureType
{
	//diffuse texture.
	DiffuseSrvHeapIndex,
	// normal texture.
	NormalSrvHeapIndex,
	//ao texture.
	AoSrvHeapIndex,
	// Metallic texture.
	MetallicSrvHeapIndex,
	//Roughness texture.
	RoughnessSrvHeapIndex,
	//Height texture.
	HeightSrvHeapIndex,
	//count
	Count
};

class TextureManage
{
public:
	TextureManage(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* CommandList, UINT CbvSrvDescriptorSize, int NumFrameResources);
	~TextureManage();

	HRESULT CreateD3DResources12(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		_In_ uint32_t resDim,
		_In_ size_t width,
		_In_ size_t height,
		_In_ size_t depth,
		_In_ size_t mipCount,
		_In_ size_t arraySize,
		_In_ DXGI_FORMAT format,
		_In_ bool forceSRGB,
		_In_ bool isCubeMap,
		_In_reads_opt_(mipCount* arraySize) D3D12_SUBRESOURCE_DATA* initData,
		ComPtr<ID3D12Resource>& texture,
		ComPtr<ID3D12Resource>& textureUploadHeap);

	HRESULT CreateTextureFromDDS12(
		_In_ ID3D12Device* device,
		_In_opt_ ID3D12GraphicsCommandList* cmdList,
		_In_ const DDS_HEADER* header,
		_In_reads_bytes_(bitSize) const uint8_t* bitData,
		_In_ size_t bitSize,
		_In_ size_t maxsize,
		_In_ bool forceSRGB,
		ComPtr<ID3D12Resource>& texture,
		ComPtr<ID3D12Resource>& textureUploadHeap);

	HRESULT CreateDDSTextureFromFile12(_In_ ID3D12Device* device,
		_In_ ID3D12GraphicsCommandList* cmdList,
		_In_z_ const wchar_t* szFileName,
		_Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
		_Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& textureUploadHeap,
		_In_ bool forceSRGB = 0,
		_In_ size_t maxsize = 0,
		_Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr);

	void CreateTexture(std::wstring name, std::wstring filename, bool ForceSRGB = true);
	std::unique_ptr<Texture>& GetTexture(std::wstring name, bool ForceSRGB = true);
	std::unique_ptr<Texture>& GetModelTexture(std::wstring name, bool ForceSRGB = true);

	void CreatePBRSRVinDescriptorHeap(
		std::vector<ID3D12Resource*> TexResource,
		int* SRVIndex,
		CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor,
		std::wstring MaterialName);

	int GetNumFrameResources() { return gNumFrameResources; }
	UINT GetCbvSrvDescriptorSize() { return mCbvSrvDescriptorSize; }
private:
	ID3D12Device* md3dDevice = nullptr;
	ID3D12GraphicsCommandList* mCommandList = nullptr;
	const int gNumFrameResources = 0;
	std::unordered_map<std::wstring, std::unique_ptr<Texture>> mTextures;
	std::wstring RootFilePath = L"../Resource/Textures/";
	UINT mCbvSrvDescriptorSize = 0;
};
