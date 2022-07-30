#include "Material.h"

//init Material
Materials* Materials::MaterialInstance=new Materials;
int Materials::MatCBIndex = 0;

void Materials::BuildMaterials(std::wstring name, float Height_Scale, XMFLOAT4 DiffuseAlbedo, XMFLOAT3 FresnelR0, XMFLOAT4X4 MatTransform, MaterialType MatType, int NumFrameResources)
{
	if (mMaterials.find(name) == mMaterials.end())
	{
		auto  Mat = std::make_unique<Material>();
		Mat->Name = name;
		Mat->MatCBIndex = MatCBIndex++;
		Mat->DiffuseAlbedo = DiffuseAlbedo;
		Mat->FresnelR0 = FresnelR0;
		Mat->MatTransform = MatTransform;
		Mat->height_scale = Height_Scale;
		Mat->NumFramesDirty = NumFrameResources;
		mMaterials[name] = std::move(Mat);


		switch (MatType)
		{
		case MaterialType::NormalPBRMat:
			mPBRTextureName.push_back(name);
			break;
		default:
			break;
		}
	}
}

Materials::Materials()
{
}

Materials::~Materials()
{
	delete MaterialInstance;
}

Materials* Materials::GetMaterialInstance()
{
	return MaterialInstance;
}
