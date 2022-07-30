#pragma once
#include "EngineUtility.h"
#include "TextureManager.h"

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
	TextureManage(ID3D12Device* d3dDevice,ID3D12GraphicsCommandList* CommandList,UINT CbvSrvDescriptorSize, int NumFrameResources);
	~TextureManage();


	void CreateTexture(std::wstring name, std::wstring filename,bool ForceSRGB=true);
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
	ID3D12Device * md3dDevice;
	ID3D12GraphicsCommandList *mCommandList;
	const int gNumFrameResources = 0;
	std::unordered_map<std::wstring, std::unique_ptr<Texture>> mTextures;
	std::wstring RootFilePath = L"../Resource/Textures/";
	UINT mCbvSrvDescriptorSize;
};

