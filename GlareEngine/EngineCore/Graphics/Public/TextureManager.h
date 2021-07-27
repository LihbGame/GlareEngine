#pragma once
#include "EngineUtility.h"
namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		class TextureManager
		{
		public:
			TextureManager() {}
			~TextureManager() {}

			void SetCommandList(ID3D12GraphicsCommandList* pCommandList);
			void CreateTexture(std::wstring name, std::wstring filename, bool ForceSRGB = true);
			std::unique_ptr<Texture>& GetTexture(std::wstring name, bool ForceSRGB = true);
		private:
			ID3D12GraphicsCommandList* mCommandList = nullptr;
			std::unordered_map<std::wstring, std::unique_ptr<Texture>> mTextures;
			std::wstring RootFilePath = L"../Resource/Textures/";
		};
	}
}