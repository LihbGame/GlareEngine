#include "Materials.h"
#include "GraphicsCore.h"

namespace GlareEngine
{
	//init Material
	MaterialManager* MaterialManager::gMaterialInstance = new MaterialManager;
	int MaterialManager::MatCBIndex = 0;


	MaterialManager* MaterialManager::GetMaterialInstance()
	{
		return gMaterialInstance;
	}

	void MaterialManager::Release()
	{
		if (gMaterialInstance)
		{
			delete gMaterialInstance;
			gMaterialInstance = nullptr;
		}
	}

	void MaterialManager::BuildMaterials(wstring name, 
		vector<Texture*>  Textures,
		float Height_Scale, XMFLOAT4 DiffuseAlbedo, 
		XMFLOAT3 FresnelR0, XMFLOAT4X4 MatTransform)
	{
		if (mMaterials.find(name) == mMaterials.end())
		{
			auto  Mat = std::make_unique<Material>();
			Mat->mName = name;
			Mat->mMatCBIndex = MatCBIndex++;
			Mat->DiffuseAlbedo = DiffuseAlbedo;
			Mat->FresnelR0 = FresnelR0;
			Mat->MatTransform = MatTransform;
			Mat->mHeightScale = Height_Scale;

			CreateSRVDescriptor(Mat.get(), Textures);
			mMaterials[name] = std::move(Mat);
		}
	}


	void MaterialManager::CreateSRVDescriptor(Material* Mat, vector<Texture*> Textures)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		for (int i = 0; i < Textures.size(); ++i)
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE Descriptor;
			Descriptor = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			srvDesc.Format = Textures[i]->Resource->GetDesc().Format;
			srvDesc.Texture2D.MipLevels = Textures[i]->Resource->GetDesc().MipLevels;
			g_Device->CreateShaderResourceView(Textures[i]->Resource.Get(), &srvDesc, Descriptor);
			Mat->mDescriptors.emplace_back(Descriptor);
		}
	}
}