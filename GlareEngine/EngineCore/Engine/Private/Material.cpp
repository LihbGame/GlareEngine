#include "Material.h"

using namespace GlareEngine;

//init Material
Materials* Materials::gMaterialInstance = new Materials;
int Materials::MatCBIndex = 0;


Materials* Materials::GetMaterialInstance()
{
	return gMaterialInstance;
}

void Materials::Release()
{
	delete gMaterialInstance;
	gMaterialInstance = nullptr;
}

void Materials::BuildMaterials(wstring name, float Height_Scale, XMFLOAT4 DiffuseAlbedo, XMFLOAT3 FresnelR0, XMFLOAT4X4 MatTransform)
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
