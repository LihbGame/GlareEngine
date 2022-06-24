#pragma once
#include "EngineUtility.h"

namespace GlareEngine
{
	class CommandContext;
	class RootSignature;


	struct PSOCommonProperty
	{
		RootSignature* pRootSignature = nullptr;
		bool IsWireframe = false;
		bool IsMSAA = false;
		int MSAACount = 1;
		int MSAAQuality = 0;
	};



	class PSO
	{
	public:

		PSO() : m_RootSignature(nullptr) {}

		static void DestroyAll(void);

		void SetRootSignature(const RootSignature& BindMappings)
		{
			m_RootSignature = &BindMappings;
		}

		const RootSignature& GetRootSignature(void) const
		{
			assert(m_RootSignature != nullptr);
			return *m_RootSignature;
		}

		ID3D12PipelineState* GetPipelineStateObject(void) const { return m_PSO; }

	protected:

		const RootSignature* m_RootSignature;

		ID3D12PipelineState* m_PSO = nullptr;
	};

	//GraphicsPSO
	class GraphicsPSO : public PSO
	{
		friend class CommandContext;

	public:

		//以空状态开始. 
		GraphicsPSO();

		void SetBlendState(const D3D12_BLEND_DESC& BlendDesc);
		void SetRasterizerState(const D3D12_RASTERIZER_DESC& RasterizerDesc);
		void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilDesc);
		void SetSampleMask(UINT SampleMask);
		void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType);
		void SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MsaaCount = 1, UINT MsaaQuality = 0);
		void SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount = 1, UINT MsaaQuality = 0);
		void SetInputLayout(UINT NumElements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs);
		void SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBProps);

		//这些const_casts不应该是必要的，但我们需要修正API以接受 "const void* pShaderBytecode" 
		void SetVertexShader(const void* Binary, size_t Size) { m_PSODesc.VS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
		void SetPixelShader(const void* Binary, size_t Size) { m_PSODesc.PS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
		void SetGeometryShader(const void* Binary, size_t Size) { m_PSODesc.GS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
		void SetHullShader(const void* Binary, size_t Size) { m_PSODesc.HS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
		void SetDomainShader(const void* Binary, size_t Size) { m_PSODesc.DS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }

		void SetVertexShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSODesc.VS = Binary; }
		void SetPixelShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSODesc.PS = Binary; }
		void SetGeometryShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSODesc.GS = Binary; }
		void SetHullShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSODesc.HS = Binary; }
		void SetDomainShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSODesc.DS = Binary; }

		//执行验证并计算一个哈希值，用于快速状态块比较。 
		void Finalize();

	private:

		D3D12_GRAPHICS_PIPELINE_STATE_DESC m_PSODesc;
		std::shared_ptr<const D3D12_INPUT_ELEMENT_DESC> m_InputLayouts;
	};

	class ComputePSO : public PSO
	{
		friend class CommandContext;

	public:
		ComputePSO();

		void SetComputeShader(const void* Binary, size_t Size) { m_PSODesc.CS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
		void SetComputeShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSODesc.CS = Binary; }

		void Finalize();

	private:

		D3D12_COMPUTE_PIPELINE_STATE_DESC m_PSODesc;
	};
}
