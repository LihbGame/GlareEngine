#include "TextureManage.h"
#include "Material.h"
TextureManage::TextureManage(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* CommandList, UINT CbvSrvDescriptorSize)
:md3dDevice(d3dDevice),
mCommandList(CommandList),
mCbvSrvDescriptorSize(CbvSrvDescriptorSize)
{
}

TextureManage::~TextureManage()
{
}

void TextureManage::CreateTexture(std::wstring name,std::wstring filename, bool ForceSRGB)
{
	// Does it already exist?
	std::unique_ptr<Texture> tex = std::make_unique<Texture>();
	if (mTextures.find(name) == mTextures.end())
	{
		CheckFileExist(filename);

		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice,
			mCommandList, filename.c_str(),
			tex.get()->Resource, tex.get()->UploadHeap, ForceSRGB));

		mTextures[name] = std::move(tex);
	}
}

std::unique_ptr<Texture>& TextureManage::GetTexture(std::wstring name, bool ForceSRGB)
{
	if (mTextures.find(name) == mTextures.end())
	{
		CreateTexture(name, RootFilePath + name + L".dds", ForceSRGB);
	}
	return mTextures[name];
}

std::unique_ptr<Texture>& TextureManage::GetModelTexture(std::wstring name, bool ForceSRGB)
{
	if (mTextures.find(name) == mTextures.end())
	{
		CreateTexture(name,name + L".dds",ForceSRGB);
	}
	return mTextures[name];
}

void TextureManage::CreatePBRSRVinDescriptorHeap(vector<ID3D12Resource*> TexResource, int* SRVIndex, CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor, wstring MaterialName)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	for (int i = 0; i < TexResource.size(); ++i)
	{
		srvDesc.Format = TexResource[i]->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = TexResource[i]->GetDesc().MipLevels;
		md3dDevice->CreateShaderResourceView(TexResource[i], &srvDesc, *hDescriptor);
		Materials::GetMaterialInstance()->GetMaterial(MaterialName)->PBRSrvHeapIndex[i] = (*SRVIndex)++;
		// next descriptor
		hDescriptor->Offset(1, mCbvSrvDescriptorSize);
	}
}
