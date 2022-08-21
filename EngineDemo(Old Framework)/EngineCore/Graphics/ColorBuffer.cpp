#include "ColorBuffer.h"
#include "GraphicsCore.h"
#include "CommandContext.h"

namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		void ColorBuffer::CreateFromSwapChain(const std::wstring& Name, ID3D12Resource* BaseResource)
		{
			AssociateWithResource(DirectX12Graphics::g_Device, Name, BaseResource, D3D12_RESOURCE_STATE_PRESENT);

			//m_UAVHandle[0] = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			//Graphics::g_Device->CreateUnorderedAccessView(m_pResource.Get(), nullptr, nullptr, m_UAVHandle[0]);

			m_RTVHandle = DirectX12Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			DirectX12Graphics::g_Device->CreateRenderTargetView(m_pResource.Get(), nullptr, m_RTVHandle);

		}

		void ColorBuffer::Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips, DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr)
		{
			NumMips = (NumMips == 0 ? ComputeNumMips(Width, Height) : NumMips);
			D3D12_RESOURCE_FLAGS Flags = CombineResourceFlags();
			D3D12_RESOURCE_DESC ResourceDesc = DescribeTex2D(Width, Height, 1, NumMips, Format, Flags);

			ResourceDesc.SampleDesc.Count = m_FragmentCount;
			ResourceDesc.SampleDesc.Quality = 0;

			D3D12_CLEAR_VALUE ClearValue = {};
			ClearValue.Format = Format;
			ClearValue.Color[0] = m_ClearColor.R();
			ClearValue.Color[1] = m_ClearColor.G();
			ClearValue.Color[2] = m_ClearColor.B();
			ClearValue.Color[3] = m_ClearColor.A();

			CreateTextureResource(DirectX12Graphics::g_Device, Name, ResourceDesc, ClearValue, VidMemPtr);
			CreateDerivedViews(DirectX12Graphics::g_Device, Format, 1, NumMips);
		}

		void ColorBuffer::CreateArray(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t MipMap, uint32_t ArrayCount, DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr)
		{
			D3D12_RESOURCE_FLAGS Flags = CombineResourceFlags();
			D3D12_RESOURCE_DESC ResourceDesc = DescribeTex2D(Width, Height, ArrayCount, MipMap, Format, Flags);

			D3D12_CLEAR_VALUE ClearValue = {};
			ClearValue.Format = Format;
			ClearValue.Color[0] = m_ClearColor.R();
			ClearValue.Color[1] = m_ClearColor.G();
			ClearValue.Color[2] = m_ClearColor.B();
			ClearValue.Color[3] = m_ClearColor.A();

			CreateTextureResource(DirectX12Graphics::g_Device, Name, ResourceDesc, ClearValue, VidMemPtr);
			CreateDerivedViews(DirectX12Graphics::g_Device, Format, ArrayCount, MipMap);
		}

		void ColorBuffer::CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips)
		{
			m_NumMipMaps = NumMips - 1;

			D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
			D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
			D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};

			RTVDesc.Format = Format;
			UAVDesc.Format = GetUAVFormat(Format);
			SRVDesc.Format = Format;
			SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			if (ArraySize > 1)
			{
				RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				RTVDesc.Texture2DArray.MipSlice = 0;
				RTVDesc.Texture2DArray.FirstArraySlice = 0;
				RTVDesc.Texture2DArray.ArraySize = (UINT)ArraySize;

				UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
				UAVDesc.Texture2DArray.MipSlice = 0;
				UAVDesc.Texture2DArray.FirstArraySlice = 0;
				UAVDesc.Texture2DArray.ArraySize = (UINT)ArraySize;

				SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				SRVDesc.Texture2DArray.MipLevels = NumMips;
				SRVDesc.Texture2DArray.MostDetailedMip = 0;
				SRVDesc.Texture2DArray.FirstArraySlice = 0;
				SRVDesc.Texture2DArray.ArraySize = (UINT)ArraySize;
			}
			else if (m_FragmentCount > 1)
			{
				RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
				SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
			}
			else
			{
				RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
				RTVDesc.Texture2D.MipSlice = 0;

				UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				UAVDesc.Texture2D.MipSlice = 0;

				SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				SRVDesc.Texture2D.MipLevels = NumMips;
				SRVDesc.Texture2D.MostDetailedMip = 0;
			}

			if (m_SRVHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
			{
				m_RTVHandle = DirectX12Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				m_SRVHandle = DirectX12Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}

			ID3D12Resource* Resource = m_pResource.Get();

			// Create the render target view
			Device->CreateRenderTargetView(Resource, &RTVDesc, m_RTVHandle);

			// Create the shader resource view
			Device->CreateShaderResourceView(Resource, &SRVDesc, m_SRVHandle);

			if (m_FragmentCount > 1)
				return;

			// Create the UAVs for each mip level (RWTexture2D)
			for (uint32_t i = 0; i < NumMips; ++i)
			{
				if (m_UAVHandle[i].ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
					m_UAVHandle[i] = DirectX12Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				Device->CreateUnorderedAccessView(Resource, nullptr, &UAVDesc, m_UAVHandle[i]);

				UAVDesc.Texture2D.MipSlice++;
			}
		}
	}
}