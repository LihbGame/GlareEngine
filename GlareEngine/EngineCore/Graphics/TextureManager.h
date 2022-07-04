#pragma once
#include "EngineUtility.h"


namespace GlareEngine
{
	struct Texture
	{
		// Unique material name for lookup.
		std::string Name;
		//only for model material 
		std::string type;

		std::string Filename;

		Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
	};

	class TextureManager
	{
	public:
		static TextureManager* GetInstance(ID3D12GraphicsCommandList* pCommandList);
		static void Shutdown();

		void CreatePBRTextures(string PathName, vector<Texture*>& Textures);

		std::unique_ptr<Texture>& GetTexture(std::wstring name, bool ForceSRGB = false);
		Texture* GetModelTexture(std::wstring name, bool ForceSRGB = false);

		void ReleaseUploadTextures();
	private:
		bool CreateTexture(std::wstring name, std::wstring filename, bool ForceSRGB = false);
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