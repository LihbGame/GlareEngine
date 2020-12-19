#include "L3DMaterial.h"

//init L3DMaterial
L3DMaterial* L3DMaterial::MaterialInstance=new L3DMaterial;
int L3DMaterial::MatCBIndex = 0;

void L3DMaterial::BuildMaterials(wstring name, float Height_Scale, XMFLOAT4 DiffuseAlbedo, XMFLOAT3 FresnelR0, XMFLOAT4X4 MatTransform, MaterialType MatType)
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

L3DMaterial::L3DMaterial()
{
}

L3DMaterial::~L3DMaterial()
{
	delete MaterialInstance;
}

L3DMaterial* L3DMaterial::GetL3DMaterialInstance()
{
	return MaterialInstance;
}
