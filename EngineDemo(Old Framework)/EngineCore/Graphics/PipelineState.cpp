#include <map>
#include <thread>
#include <mutex>
#include "GraphicsCore.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "Hash.h"

using namespace GlareEngine::DirectX12Graphics;
using namespace Microsoft::WRL;

static std::map< size_t, ComPtr<ID3D12PipelineState> > s_GraphicsPSOHashMap;
static std::map< size_t, ComPtr<ID3D12PipelineState> > s_ComputePSOHashMap;
namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		void PSO::DestroyAll(void)
		{
			s_GraphicsPSOHashMap.clear();
			s_ComputePSOHashMap.clear();
		}
	}
}


GlareEngine::DirectX12Graphics::GraphicsPSO::GraphicsPSO()
{
	ZeroMemory(&m_PSODesc, sizeof(m_PSODesc));
	m_PSODesc.NodeMask = 1;
	m_PSODesc.SampleMask = 0xFFFFFFFFu;
	m_PSODesc.SampleDesc.Count = 1;
	m_PSODesc.InputLayout.NumElements = 0;
}

void GlareEngine::DirectX12Graphics::GraphicsPSO::SetBlendState(const D3D12_BLEND_DESC& BlendDesc)
{
	m_PSODesc.BlendState = BlendDesc;
}

void GlareEngine::DirectX12Graphics::GraphicsPSO::SetRasterizerState(const D3D12_RASTERIZER_DESC& RasterizerDesc)
{
	m_PSODesc.RasterizerState = RasterizerDesc;
}

void GlareEngine::DirectX12Graphics::GraphicsPSO::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilDesc)
{
	m_PSODesc.DepthStencilState = DepthStencilDesc;
}

void GlareEngine::DirectX12Graphics::GraphicsPSO::SetSampleMask(UINT SampleMask)
{
	m_PSODesc.SampleMask = SampleMask;
}

void GlareEngine::DirectX12Graphics::GraphicsPSO::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType)
{
	//Can't draw with undefined topology
	assert(TopologyType != D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED);
	m_PSODesc.PrimitiveTopologyType = TopologyType;
}

void GlareEngine::DirectX12Graphics::GraphicsPSO::SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
{
	SetRenderTargetFormats(1, &RTVFormat, DSVFormat, MsaaCount, MsaaQuality);
}

void GlareEngine::DirectX12Graphics::GraphicsPSO::SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
{
	//Null format array conflicts with non-zero length
	assert(NumRTVs == 0 || RTVFormats != nullptr);
	for (UINT i = 0; i < NumRTVs; ++i)
		m_PSODesc.RTVFormats[i] = RTVFormats[i];
	for (UINT i = NumRTVs; i < m_PSODesc.NumRenderTargets; ++i)
		m_PSODesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
	m_PSODesc.NumRenderTargets = NumRTVs;
	m_PSODesc.DSVFormat = DSVFormat;
	m_PSODesc.SampleDesc.Count = MsaaCount;
	m_PSODesc.SampleDesc.Quality = MsaaQuality;
}

void GlareEngine::DirectX12Graphics::GraphicsPSO::SetInputLayout(UINT NumElements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs)
{
	m_PSODesc.InputLayout.NumElements = NumElements;

	if (NumElements > 0)
	{
		D3D12_INPUT_ELEMENT_DESC* NewElements = (D3D12_INPUT_ELEMENT_DESC*)malloc(sizeof(D3D12_INPUT_ELEMENT_DESC) * NumElements);
		memcpy(NewElements, pInputElementDescs, NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC));
		m_InputLayouts.reset((const D3D12_INPUT_ELEMENT_DESC*)NewElements);
	}
	else
		m_InputLayouts = nullptr;
}

void GlareEngine::DirectX12Graphics::GraphicsPSO::SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBProps)
{
	m_PSODesc.IBStripCutValue = IBProps;
}

void GlareEngine::DirectX12Graphics::GraphicsPSO::Finalize()
{
	// Make sure the root signature is finalized first
	m_PSODesc.pRootSignature = m_RootSignature->GetSignature();
	assert(m_PSODesc.pRootSignature != nullptr);

	m_PSODesc.InputLayout.pInputElementDescs = nullptr;
	size_t HashCode = HashState(&m_PSODesc);
	HashCode = HashState(m_InputLayouts.get(), m_PSODesc.InputLayout.NumElements, HashCode);
	m_PSODesc.InputLayout.pInputElementDescs = m_InputLayouts.get();

	ID3D12PipelineState** PSORef = nullptr;
	bool firstCompile = false;
	{
		static std::mutex s_HashMapMutex;
		std::lock_guard<std::mutex> CS(s_HashMapMutex);
		auto iter = s_GraphicsPSOHashMap.find(HashCode);

		// Reserve space so the next inquiry will find that someone got here first.
		if (iter == s_GraphicsPSOHashMap.end())
		{
			firstCompile = true;
			PSORef = s_GraphicsPSOHashMap[HashCode].GetAddressOf();
		}
		else
			PSORef = iter->second.GetAddressOf();
	}

	if (firstCompile)
	{
		ThrowIfFailed(DirectX12Graphics::g_Device->CreateGraphicsPipelineState(&m_PSODesc, IID_PPV_ARGS(&m_PSO)));
		s_GraphicsPSOHashMap[HashCode].Attach(m_PSO);
	}
	else
	{
		while (*PSORef == nullptr)
			std::this_thread::yield();
		m_PSO = *PSORef;
	}
}

ComputePSO::ComputePSO()
{
	ZeroMemory(&m_PSODesc, sizeof(m_PSODesc));
	m_PSODesc.NodeMask = 1;
}

void ComputePSO::Finalize()
{
	// Make sure the root signature is finalized first
	m_PSODesc.pRootSignature = m_RootSignature->GetSignature();
	assert(m_PSODesc.pRootSignature != nullptr);

	size_t HashCode = HashState(&m_PSODesc);

	ID3D12PipelineState** PSORef = nullptr;
	bool firstCompile = false;
	{
		static std::mutex s_HashMapMutex;
		std::lock_guard<std::mutex> CS(s_HashMapMutex);
		auto iter = s_ComputePSOHashMap.find(HashCode);

		// Reserve space so the next inquiry will find that someone got here first.
		if (iter == s_ComputePSOHashMap.end())
		{
			firstCompile = true;
			PSORef = s_ComputePSOHashMap[HashCode].GetAddressOf();
		}
		else
			PSORef = iter->second.GetAddressOf();
	}

	if (firstCompile)
	{
		ThrowIfFailed(DirectX12Graphics::g_Device->CreateComputePipelineState(&m_PSODesc, IID_PPV_ARGS(&m_PSO)));
		s_ComputePSOHashMap[HashCode].Attach(m_PSO);
	}
	else
	{
		while (*PSORef == nullptr)
			std::this_thread::yield();
		m_PSO = *PSORef;
	}
}
