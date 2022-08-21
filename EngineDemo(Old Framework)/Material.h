#pragma once
#include "EngineUtility.h"
#include "FrameResource.h"

// Simple struct to represent a material for our demos.  A production 3D engine
// would likely create a class hierarchy of Materials.
struct Material
{
	// Unique material name for lookup.
	std::wstring Name;

	// Index into constant buffer corresponding to this material.
	int MatCBIndex = -1;

	//Texture Srv Heap Index.
	int PBRSrvHeapIndex[6];

	float height_scale = 0.0f;
	// Dirty flag indicating the material has changed and we need to update the constant buffer.
	// Because we have a material constant buffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify a material we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = 0;

	// Material constant buffer data used for shading.
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};



enum class MaterialType : int
{
	NormalPBRMat = 0,
	HeightMapTerrainPBRMat,
	ModelPBRMat,
	SkyMat,
	Count
};

class Materials
{
public:
	
	~Materials();

	static Materials* GetMaterialInstance();

	static int GetMaterialSize() { return MatCBIndex; }

	void BuildMaterials(
		std::wstring name,
		float Height_Scale,
		XMFLOAT4 DiffuseAlbedo,
		XMFLOAT3 FresnelR0,
		XMFLOAT4X4 MatTransform,
		MaterialType MatType,
		int NumFrameResources);


	::Material* GetMaterial(std::wstring MaterialName) { return mMaterials[MaterialName].get(); }


	std::unordered_map<std::wstring, std::unique_ptr<::Material>>& GetAllMaterial() { return mMaterials; };
private:
	Materials();
	
	static  Materials* MaterialInstance;

	static int MatCBIndex;

	std::vector<std::wstring> mPBRTextureName;

	std::unordered_map<std::wstring, std::unique_ptr<::Material>> mMaterials;
};

