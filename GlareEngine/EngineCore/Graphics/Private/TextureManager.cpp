#include "TextureManager.h"
#include "GraphicsCore.h"

namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		void TextureManager::SetCommandList(ID3D12GraphicsCommandList* pCommandList)
		{
			mCommandList = pCommandList;
		}

		void TextureManager::CreateTexture(std::wstring name, std::wstring filename, bool ForceSRGB)
		{
			// Does it already exist?
			std::unique_ptr<Texture> tex = std::make_unique<Texture>();
			if (mTextures.find(name) == mTextures.end())
			{
				CheckFileExist(filename);

				ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(GlareEngine::DirectX12Graphics::g_Device,
					mCommandList, filename.c_str(),
					tex.get()->Resource, tex.get()->UploadHeap, ForceSRGB));

				mTextures[name] = std::move(tex);
			}
		}

		std::unique_ptr<Texture>& TextureManager::GetTexture(std::wstring name, bool ForceSRGB)
		{
			if (mTextures.find(name) == mTextures.end())
			{
				CreateTexture(name, RootFilePath + name + L".dds", ForceSRGB);
			}
			return mTextures[name];
		}
	}
}