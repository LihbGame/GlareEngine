#pragma once
#include "EngineUtility.h"
#include "TextureManager.h"

namespace GlareEngine
{
	struct Material
	{
		// Unique material name for lookup.
		std::wstring                            mName;
		// Index into constant buffer corresponding to this material.
		int                                     mMatCBIndex = -1;
		//For Height map
		float                                   mHeightScale = 0.0f;
		//Albedo
		DirectX::XMFLOAT4                       DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		//Fresnel R0
		DirectX::XMFLOAT3                       FresnelR0 = { 0.01f, 0.01f, 0.01f };
		//Material Transform
		DirectX::XMFLOAT4X4                     MatTransform = MathHelper::Identity4x4();
		//Texture SRV Index
		std::vector<int>								mDescriptorsIndex;
	};


	//GPU struct Data
	struct MaterialConstant
	{
		DirectX::XMFLOAT4			mDiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		DirectX::XMFLOAT3			mFresnelR0 = { 0.01f, 0.01f, 0.01f };
		float						mHeightScale = 0.0f;
		DirectX::XMFLOAT4X4			mMatTransform = MathHelper::Identity4x4();
		UINT						mRoughnessMapIndex = 0;
		UINT						mDiffuseMapIndex = 0;
		UINT						mNormalMapIndex = 0;
		UINT						mMetallicMapIndex = 0;
		UINT						mAOMapIndex = 0;
		UINT						mHeightMapIndex = 0;
	};


	class MaterialManager
	{
	public:

		~MaterialManager() {};

		static MaterialManager* GetMaterialInstance();

		static void Release();

		int GetMaterialSize() { return MatCBIndex; }

		void CreateMaterialsConstantBuffer();

		std::vector<MaterialConstant>& GetMaterialsConstantBuffer();

		void BuildMaterials(
			std::wstring name,
			std::vector<Texture*>   Textures,
			float Height_Scale = 0.0f,
			XMFLOAT4 DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
			XMFLOAT3 FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f),
			XMFLOAT4X4 MatTransform = MathHelper::Identity4x4());


		const Material* GetMaterial(std::wstring MaterialName) { 
			if (mMaterials.find(MaterialName) == mMaterials.end())
			{
				return nullptr;
			}
			return mMaterials[MaterialName].get(); 
		}


		std::unordered_map<std::wstring, std::unique_ptr<Material>>& GetAllMaterial() { return mMaterials; };
	private:
		MaterialManager() {}

		void CreateSRVDescriptor(Material* Mat, std::vector<Texture*> texture);

		static  MaterialManager* gMaterialInstance;

		static int MatCBIndex;

		std::unordered_map<std::wstring, std::unique_ptr<Material>> mMaterials;
	
		std::vector<MaterialConstant> mMaterialConstanrBuffer;
	};
}
