#pragma once
#include "L3DUtil.h"

class L3DTextureManage
{
public:
	L3DTextureManage(ID3D12Device* d3dDevice,ID3D12GraphicsCommandList* CommandList);
	~L3DTextureManage();


	void CreateTexture(std::wstring name, std::wstring filename);
	std::unique_ptr<Texture>& GetTexture(std::wstring name);
	std::unique_ptr<Texture>& GetModelTexture(std::wstring name);
private:
	ID3D12Device * md3dDevice;
	ID3D12GraphicsCommandList *mCommandList;
	std::unordered_map<std::wstring, std::unique_ptr<Texture>> mTextures;
	std::wstring RootFilePath = L"Textures/";
};

