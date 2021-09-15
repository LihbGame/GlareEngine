#include "Materials.h"

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
		delete gMaterialInstance;
		gMaterialInstance = nullptr;
	}

	void MaterialManager::BuildMaterials(wstring name, float Height_Scale, XMFLOAT4 DiffuseAlbedo, XMFLOAT3 FresnelR0, XMFLOAT4X4 MatTransform)
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
			mMaterials[name] = std::move(Mat);
		}
	}
}