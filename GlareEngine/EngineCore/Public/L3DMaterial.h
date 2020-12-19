#pragma once
#include "L3DUtil.h"

enum class MaterialType : int
{
	NormalPBRMat = 0,
	HeightMapTerrainPBRMat,
	ModelPBRMat,
	SkyMat,
	Count
};

class L3DMaterial
{
public:
	
	~L3DMaterial();

	static L3DMaterial* GetL3DMaterialInstance();

	static int GetMaterialSize() { return MatCBIndex; }

	void BuildMaterials(
		wstring name,
		float Height_Scale,
		XMFLOAT4 DiffuseAlbedo,
		XMFLOAT3 FresnelR0,
		XMFLOAT4X4 MatTransform,
		MaterialType MatType);


	Material* GetMaterial(std::wstring MaterialName) { return mMaterials[MaterialName].get(); }


	std::unordered_map<std::wstring, std::unique_ptr<Material>>& GetAllMaterial() { return mMaterials; };
private:
	L3DMaterial();
	
	static  L3DMaterial* MaterialInstance;

	static int MatCBIndex;

	std::vector<wstring> mPBRTextureName;

	std::unordered_map<std::wstring, std::unique_ptr<Material>> mMaterials;
};

