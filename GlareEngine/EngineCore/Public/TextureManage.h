#pragma once
#include "EngineUtility.h"

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
	TextureManage(ID3D12Device* d3dDevice,ID3D12GraphicsCommandList* CommandList,UINT CbvSrvDescriptorSize);
	~TextureManage();


	void CreateTexture(std::wstring name, std::wstring filename,bool ForceSRGB=true);
	std::unique_ptr<Texture>& GetTexture(std::wstring name, bool ForceSRGB = true);
	std::unique_ptr<Texture>& GetModelTexture(std::wstring name, bool ForceSRGB = true);

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
	std::wstring RootFilePath = L"../Resource/Textures/";
	UINT mCbvSrvDescriptorSize;
};

