#include "TextureManager.h"
#include "GraphicsCore.h"

string  PBRTextureFileType[] = {
	"_albedo",
	"_normal",
	"_ao",
	"_metallic",
	"_roughness",
	"_height"
};




namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		TextureManager* TextureManager::m_pTextureManagerInstance = nullptr;
		ID3D12GraphicsCommandList* TextureManager::mCommandList = nullptr;

		TextureManager* TextureManager::GetInstance(ID3D12GraphicsCommandList* pCommandList)
		{
			if (m_pTextureManagerInstance == nullptr)
			{
				m_pTextureManagerInstance = new TextureManager();
			}
			//reset command list 
			mCommandList = pCommandList;

			return m_pTextureManagerInstance;
		}

		void TextureManager::Shutdown()
		{
			if (m_pTextureManagerInstance)
			{
				delete m_pTextureManagerInstance;
				m_pTextureManagerInstance = nullptr;
			}
		}

		void TextureManager::CreatePBRTextures(string PathName, vector<Texture*>& Textures)
		{
			string Fullfilename = "";
			bool bSRGB = true;
			for (auto Type : PBRTextureFileType)
			{
				Fullfilename = PathName + Type;
				if (Type == "_normal")
				{
					bSRGB = false;
				}
				else
				{
					bSRGB = true;
				}
				Textures.push_back(GetModelTexture(StringToWString(Fullfilename), bSRGB));
			}
		}

		bool TextureManager::CreateTexture(std::wstring name, std::wstring filename, bool ForceSRGB)
		{
			if (CheckFileExist(filename))
			{
				std::unique_ptr<Texture> tex = std::make_unique<Texture>();
				ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(GlareEngine::DirectX12Graphics::g_Device,
					mCommandList, filename.c_str(),
					tex.get()->Resource, tex.get()->UploadHeap, ForceSRGB));
				mTextures[name] = std::move(tex);
				return true;
			}
			return false;
		}

		std::unique_ptr<Texture>& TextureManager::GetTexture(std::wstring name, bool ForceSRGB)
		{
			if (mTextures.find(name) == mTextures.end())
			{
				CreateTexture(name, RootFilePath + name + L".dds", ForceSRGB);
			}
			return mTextures[name];
		}

		Texture* TextureManager::GetModelTexture(std::wstring name, bool ForceSRGB)
		{
			bool isTextureCreated = true;
			if (mTextures.find(name) == mTextures.end())
			{
				isTextureCreated = CreateTexture(name, name + L".dds", ForceSRGB);
			}
			return isTextureCreated ? mTextures[name].get() : nullptr;
		}

		void TextureManager::ReleaseUploadTextures()
		{
			for (auto &texture: mTextures)
			{
				texture.second.get()->UploadHeap.Get()->Release();
				texture.second.get()->UploadHeap.Detach();
			}
		}
	}
}