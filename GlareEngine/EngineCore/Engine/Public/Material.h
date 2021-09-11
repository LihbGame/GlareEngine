#pragma once
#include "EngineUtility.h"

namespace GlareEngine
{
	struct Material
	{
		// Unique material name for lookup.
		std::wstring mName;

		// Index into constant buffer corresponding to this material.
		int mMatCBIndex = -1;

		float mHeightScale = 0.0f;

		// Material constant buffer data used for shading.
		DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		
		DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
		
		DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
	};


	class Materials
	{
	public:

		~Materials() {};

		static Materials* GetMaterialInstance();
		static void Release();

		static int GetMaterialSize() { return MatCBIndex; }

		void BuildMaterials(
			wstring name,
			float Height_Scale,
			XMFLOAT4 DiffuseAlbedo,
			XMFLOAT3 FresnelR0,
			XMFLOAT4X4 MatTransform);


		Material* GetMaterial(std::wstring MaterialName) { return mMaterials[MaterialName].get(); }


		std::unordered_map<std::wstring, std::unique_ptr<Material>>& GetAllMaterial() { return mMaterials; };
	private:
		Materials() {}

		static  Materials* gMaterialInstance;

		static int MatCBIndex;

		std::unordered_map<std::wstring, std::unique_ptr<Material>> mMaterials;
	};
}
