#include <map>
#include <thread>
#include <mutex>
#include "GraphicsCore.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "Math/Hash.h"
#include "Engine/EngineLog.h"

using namespace std;
using Microsoft::WRL::ComPtr;


namespace GlareEngine
{
	static map< size_t, ComPtr<ID3D12PipelineState> > gGraphicsPSOHashMap;
	static map< size_t, ComPtr<ID3D12PipelineState> > gComputePSOHashMap;


	void PSO::DestroyAll(void)
	{
		gGraphicsPSOHashMap.clear();
		gComputePSOHashMap.clear();
	}
}


GlareEngine::GraphicsPSO::GraphicsPSO(wstring psoName)
{
	m_Name = psoName;
	ZeroMemory(&m_PSODesc, sizeof(m_PSODesc));
	m_PSODesc.NodeMask = 1;
	m_PSODesc.SampleMask = 0xFFFFFFFFu;
	m_PSODesc.SampleDesc.Count = 1;
	m_PSODesc.InputLayout.NumElements = 0;
}

GraphicsPSO& GlareEngine::GraphicsPSO::RuntimePSOCopy(const GraphicsPSO& copy)
{
	m_PSODesc= copy.m_PSODesc;
	SetInputLayout(GetPSODesc().InputLayout.NumElements, copy.GetInputLayout());
	SetRootSignature(copy.GetRootSignature());
	return *this;
}

void GlareEngine::GraphicsPSO::SetBlendState(const D3D12_BLEND_DESC& BlendDesc)
{
	m_PSODesc.BlendState = BlendDesc;
}

void GlareEngine::GraphicsPSO::SetRasterizerState(const D3D12_RASTERIZER_DESC& RasterizerDesc)
{
	m_PSODesc.RasterizerState = RasterizerDesc;
}

void GlareEngine::GraphicsPSO::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilDesc)
{
	m_PSODesc.DepthStencilState = DepthStencilDesc;
}

void GlareEngine::GraphicsPSO::SetSampleMask(UINT SampleMask)
{
	m_PSODesc.SampleMask = SampleMask;
}

void GlareEngine::GraphicsPSO::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType)
{
	//Can't draw with undefined topology
	assert(TopologyType != D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED);
	m_PSODesc.PrimitiveTopologyType = TopologyType;
}

void GlareEngine::GraphicsPSO::SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
{
	SetRenderTargetFormats(1, &RTVFormat, DSVFormat, MsaaCount, MsaaQuality);
}

void GlareEngine::GraphicsPSO::SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
{
	//Null format array conflicts with non-zero length
	assert(NumRTVs == 0 || RTVFormats != nullptr);
	for (UINT i = 0; i < NumRTVs; ++i)
	{
		assert(RTVFormats[i] != DXGI_FORMAT_UNKNOWN);
		m_PSODesc.RTVFormats[i] = RTVFormats[i];
	}
	for (UINT i = NumRTVs; i < m_PSODesc.NumRenderTargets; ++i)
	{
		m_PSODesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
	}
	m_PSODesc.NumRenderTargets = NumRTVs;
	m_PSODesc.DSVFormat = DSVFormat;
	m_PSODesc.SampleDesc.Count = MsaaCount;
	m_PSODesc.SampleDesc.Quality = MsaaQuality;
}

void GlareEngine::GraphicsPSO::SetInputLayout(UINT NumElements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs)
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

void GlareEngine::GraphicsPSO::SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBProps)
{
	m_PSODesc.IBStripCutValue = IBProps;
}

void GlareEngine::GraphicsPSO::Finalize()
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
		static mutex s_HashMapMutex;
		lock_guard<mutex> CS(s_HashMapMutex);
		auto iter = gGraphicsPSOHashMap.find(HashCode);

		// Reserve space so the next inquiry will find that someone got here first.
		if (iter == gGraphicsPSOHashMap.end())
		{
			firstCompile = true;
			PSORef = gGraphicsPSOHashMap[HashCode].GetAddressOf();
		}
		else
			PSORef = iter->second.GetAddressOf();
	}

	if (firstCompile)
	{
		ThrowIfFailed(g_Device->CreateGraphicsPipelineState(&m_PSODesc, IID_PPV_ARGS(&m_PSO)));
		gGraphicsPSOHashMap[HashCode].Attach(m_PSO);
	}
	else
	{
		while (*PSORef == nullptr)
			EngineLog::AddLog(L"Can not find or create GraphicsPSO: %s !",this->m_Name);
		m_PSO = *PSORef;
	}
}

ComputePSO::ComputePSO(wstring psoName)
{
	m_Name = psoName;
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
		static mutex s_HashMapMutex;
		lock_guard<mutex> CS(s_HashMapMutex);
		auto iter = gComputePSOHashMap.find(HashCode);

		// Reserve space so the next inquiry will find that someone got here first.
		if (iter == gComputePSOHashMap.end())
		{
			firstCompile = true;
			PSORef = gComputePSOHashMap[HashCode].GetAddressOf();
		}
		else
			PSORef = iter->second.GetAddressOf();
	}

	if (firstCompile)
	{
		ThrowIfFailed(g_Device->CreateComputePipelineState(&m_PSODesc, IID_PPV_ARGS(&m_PSO)));
		gComputePSOHashMap[HashCode].Attach(m_PSO);
	}
	else
	{
		while (*PSORef == nullptr)
			this_thread::yield();
		m_PSO = *PSORef;
	}
}
