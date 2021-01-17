#pragma once
#include "PixelBuffer.h"
namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		class DepthBuffer :
			public PixelBuffer
		{
		public:
			DepthBuffer(float ClearDepth = 0.0f, uint8_t ClearStencil = 0)
				: m_ClearDepth(ClearDepth), m_ClearStencil(ClearStencil)
			{
				m_hDSV[0].ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
				m_hDSV[1].ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
				m_hDSV[2].ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
				m_hDSV[3].ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
				m_hDepthSRV.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
				m_hStencilSRV.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
			}

			//创建一个深度缓冲区。 如果提供了一个地址，内存将不会被分配。vmem地址允许你对缓冲区进行别名。 
			void Create(const std::wstring& Name, uint32_t Width, uint32_t Height, DXGI_FORMAT Format,
				D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);


			void Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumSamples, DXGI_FORMAT Format,
				D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);


			// 获取预先创建的CPU可见描述符句柄。
			const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSV() const { return m_hDSV[0]; }
			const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSV_DepthReadOnly() const { return m_hDSV[1]; }
			const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSV_StencilReadOnly() const { return m_hDSV[2]; }
			const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSV_ReadOnly() const { return m_hDSV[3]; }
			const D3D12_CPU_DESCRIPTOR_HANDLE& GetDepthSRV() const { return m_hDepthSRV; }
			const D3D12_CPU_DESCRIPTOR_HANDLE& GetStencilSRV() const { return m_hStencilSRV; }

			float GetClearDepth() const { return m_ClearDepth; }
			uint8_t GetClearStencil() const { return m_ClearStencil; }

		private:

			void CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format);

			float m_ClearDepth;
			uint8_t m_ClearStencil;
			D3D12_CPU_DESCRIPTOR_HANDLE m_hDSV[4];
			D3D12_CPU_DESCRIPTOR_HANDLE m_hDepthSRV;
			D3D12_CPU_DESCRIPTOR_HANDLE m_hStencilSRV;
		};
	}
}
