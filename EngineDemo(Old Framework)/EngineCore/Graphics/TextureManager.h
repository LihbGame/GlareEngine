#pragma once
#include "EngineUtility.h"

#include <DDS.h>

namespace GlareEngine
{
	struct Texture
	{
		// Unique material name for lookup.
		std::wstring Name;
		//only for model material 
		std::wstring type;

		std::wstring Filename;

		Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
	};


	namespace DirectX12Graphics
	{
		class TextureManager
		{
		public:
			static TextureManager* GetInstance(ID3D12GraphicsCommandList* pCommandList);
			static void Shutdown();

			void CreatePBRTextures(std::wstring PathName, std::vector<Texture*>& Textures);

			std::unique_ptr<Texture>& GetTexture(std::wstring name, bool ForceSRGB = true);
			Texture* GetModelTexture(std::wstring name, bool ForceSRGB = true);

			//void ReleaseUploadTextures();
		private:
			HRESULT CreateD3DResources12(
				ID3D12Device* device,
				ID3D12GraphicsCommandList* cmdList,
				_In_ uint32_t resDim,
				_In_ size_t width,
				_In_ size_t height,
				_In_ size_t depth,
				_In_ size_t mipCount,
				_In_ size_t arraySize,
				_In_ DXGI_FORMAT format,
				_In_ bool forceSRGB,
				_In_ bool isCubeMap,
				_In_reads_opt_(mipCount* arraySize) D3D12_SUBRESOURCE_DATA* initData,
				ComPtr<ID3D12Resource>& texture,
				ComPtr<ID3D12Resource>& textureUploadHeap
			);

			HRESULT CreateTextureFromDDS12(
				_In_ ID3D12Device* device,
				_In_opt_ ID3D12GraphicsCommandList* cmdList,
				_In_ const DDS_HEADER* header,
				_In_reads_bytes_(bitSize) const uint8_t* bitData,
				_In_ size_t bitSize,
				_In_ size_t maxsize,
				_In_ bool forceSRGB,
				ComPtr<ID3D12Resource>& texture,
				ComPtr<ID3D12Resource>& textureUploadHeap);

			HRESULT CreateDDSTextureFromFile12(_In_ ID3D12Device* device,
				_In_ ID3D12GraphicsCommandList* cmdList,
				_In_z_ const wchar_t* szFileName,
				_Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
				_Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& textureUploadHeap,
				_In_ bool forceSRGB = 0,
				_In_ size_t maxsize = 0,
				_Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr
			);
			
			bool CreateTexture(std::wstring name, std::wstring filename, bool ForceSRGB = true);
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