#pragma once
#include "DepthBuffer.h"


namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		class GraphicsContext;

		class ShadowBuffer :
			public DepthBuffer
		{
		public:
			ShadowBuffer() {}

			void Create(const std::wstring& Name, uint32_t Width, uint32_t Height, DXGI_FORMAT Format= DXGI_FORMAT_D16_UNORM,
				D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

			D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return GetDepthSRV(); }

			void BeginRendering(GraphicsContext& context);
			void EndRendering(GraphicsContext& context);

		private:
			D3D12_VIEWPORT m_Viewport;
			D3D12_RECT m_Scissor;
		};
	}
}

