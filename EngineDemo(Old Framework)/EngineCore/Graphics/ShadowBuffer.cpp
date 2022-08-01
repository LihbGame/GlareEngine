#include "ShadowBuffer.h"
#include "CommandContext.h"

namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		void ShadowBuffer::Create(const std::wstring& Name, uint32_t Width, uint32_t Height, DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr)
		{
			DepthBuffer::Create(Name, Width, Height, Format, false, VidMemPtr);

			m_Viewport.TopLeftX = 0.0f;
			m_Viewport.TopLeftY = 0.0f;
			m_Viewport.Width = (float)Width;
			m_Viewport.Height = (float)Height;
			m_Viewport.MinDepth = 0.0f;
			m_Viewport.MaxDepth = 1.0f;

			//防止绘制到边界像素，这样我们就不必担心阴影拉伸
			m_Scissor.left = 1;
			m_Scissor.top = 1;
			m_Scissor.right = (LONG)Width - 2;
			m_Scissor.bottom = (LONG)Height - 2;
		}


		void ShadowBuffer::BeginRendering(GraphicsContext& Context)
		{
			Context.TransitionResource(*this, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
			Context.ClearDepth(*this);
			Context.SetDepthStencilTarget(GetDSV());
			Context.SetViewportAndScissor(m_Viewport, m_Scissor);
		}

		void ShadowBuffer::EndRendering(GraphicsContext& Context)
		{
			Context.TransitionResource(*this, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
	}
}