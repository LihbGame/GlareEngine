#pragma once
#include "EngineUtility.h"
namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		class TextureManager
		{
		public:
			static TextureManager* GetInstance(ID3D12GraphicsCommandList* pCommandList);
			static void Shutdown();


			void CreatePBRTextures(string PathName, vector<Texture*>& Textures);
			void CreateTexture(std::wstring name, std::wstring filename, bool ForceSRGB = true);
			std::unique_ptr<Texture>& GetTexture(std::wstring name, bool ForceSRGB = true);
			std::unique_ptr<Texture>& GetModelTexture(std::wstring name, bool ForceSRGB = true);

		private:
			
			std::unordered_map<std::wstring, std::unique_ptr<Texture>> mTextures;
			std::wstring RootFilePath = L"../Resource/Textures/";
		
			static TextureManager* m_pTextureManagerInstance;
			static ID3D12GraphicsCommandList* mCommandList;

		private:
			TextureManager() {}
			~TextureManager() {}
		};
	}
}