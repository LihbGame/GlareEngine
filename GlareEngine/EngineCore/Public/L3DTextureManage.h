#pragma once
#include "L3DUtil.h"

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

class L3DTextureManage
{
public:
	L3DTextureManage(ID3D12Device* d3dDevice,ID3D12GraphicsCommandList* CommandList,UINT CbvSrvDescriptorSize);
	~L3DTextureManage();


	void CreateTexture(std::wstring name, std::wstring filename);
	std::unique_ptr<Texture>& GetTexture(std::wstring name);
	std::unique_ptr<Texture>& GetModelTexture(std::wstring name);

	void CreatePBRSRVinDescriptorHeap(
		vector<ID3D12Resource*> TexResource,
		int* SRVIndex,
		CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor,
		wstring MaterialName);

	UINT GetCbvSrvDescriptorSize() { return mCbvSrvDescriptorSize; }
private:
	ID3D12Device * md3dDevice;
	ID3D12GraphicsCommandList *mCommandList;
	std::unordered_map<std::wstring, std::unique_ptr<Texture>> mTextures;
	std::wstring RootFilePath = L"Textures/";
	UINT mCbvSrvDescriptorSize;
};

