#pragma once
#include "EngineUtility.h"

namespace GlareEngine
{
	using namespace std;
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
		//Texture
		vector<CD3DX12_CPU_DESCRIPTOR_HANDLE>	mDescriptors;
	};


	class MaterialManager
	{
	public:

		~MaterialManager() {};

		static MaterialManager* GetMaterialInstance();

		void Release();

		int GetMaterialSize() { return MatCBIndex; }

		void BuildMaterials(
			wstring name,
			vector<Texture*>   Textures,
			float Height_Scale = 0.0f,
			XMFLOAT4 DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
			XMFLOAT3 FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f),
			XMFLOAT4X4 MatTransform = MathHelper::Identity4x4());


		const Material* GetMaterial(std::wstring MaterialName) { return mMaterials[MaterialName].get(); }


		std::unordered_map<std::wstring, std::unique_ptr<Material>>& GetAllMaterial() { return mMaterials; };
	private:
		MaterialManager() {}

		void CreateSRVDescriptor(Material* Mat, vector<Texture*> texture);

		static  MaterialManager* gMaterialInstance;

		static int MatCBIndex;

		std::unordered_map<std::wstring, std::unique_ptr<Material>> mMaterials;
	};
}
