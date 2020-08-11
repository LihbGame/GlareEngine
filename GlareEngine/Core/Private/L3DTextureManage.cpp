#include "L3DTextureManage.h"

L3DTextureManage::L3DTextureManage(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* CommandList)
:md3dDevice(d3dDevice),
mCommandList(CommandList)
{
}

L3DTextureManage::~L3DTextureManage()
{
}

void L3DTextureManage::CreateTexture(std::wstring name,std::wstring filename)
{
	// Does it already exist?
	std::unique_ptr<Texture> tex = std::make_unique<Texture>();
	if (mTextures.find(name) == mTextures.end())
	{
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice,
			mCommandList, filename.c_str(),
			tex.get()->Resource, tex.get()->UploadHeap));

		mTextures[name] = std::move(tex);
	}
}

std::unique_ptr<Texture>& L3DTextureManage::GetTexture(std::wstring name)
{
	if (mTextures.find(name) == mTextures.end())
	{
		CreateTexture(name, RootFilePath + name + L".dds");
	}
	return mTextures[name];
}
