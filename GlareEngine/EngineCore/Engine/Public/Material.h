#pragma once
#include "EngineUtility.h"

namespace GlareEngine
{
	class Materials
	{
	public:

		~Materials() {};

		static Materials* GetMaterialInstance();
		static void ReleaseMaterialInstance();

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
		Materials();

		static  Materials* MaterialInstance;

		static int MatCBIndex;

		std::vector<wstring> mPBRTextureName;

		std::unordered_map<std::wstring, std::unique_ptr<Material>> mMaterials;
	};
}
