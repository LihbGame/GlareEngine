#include <map>
#include <thread>
#include <mutex>
#include "GraphicsCore.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "Hash.h"

using namespace std;
using namespace GlareEngine::Graphics;
using Microsoft::WRL::ComPtr;

static map< size_t, ComPtr<ID3D12PipelineState> > s_GraphicsPSOHashMap;
static map< size_t, ComPtr<ID3D12PipelineState> > s_ComputePSOHashMap;

void GlareEngine::PSO::DestroyAll(void)
{
	s_GraphicsPSOHashMap.clear();
	s_ComputePSOHashMap.clear();
}

GlareEngine::GraphicsPSO::GraphicsPSO()
{
	ZeroMemory(&m_PSODesc, sizeof(m_PSODesc));
	m_PSODesc.NodeMask = 1;
	m_PSODesc.SampleMask = 0xFFFFFFFFu;
	m_PSODesc.SampleDesc.Count = 1;
	m_PSODesc.InputLayout.NumElements = 0;
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
	assert(TopologyType != D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED, "Can't draw with undefined topology");
	m_PSODesc.PrimitiveTopologyType = TopologyType;
}

void GlareEngine::GraphicsPSO::SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
{
	SetRenderTargetFormats(1, &RTVFormat, DSVFormat, MsaaCount, MsaaQuality);
}

void GlareEngine::GraphicsPSO::SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
{
	assert(NumRTVs == 0 || RTVFormats != nullptr, "Null format array conflicts with non-zero length");
	for (UINT i = 0; i < NumRTVs; ++i)
		m_PSODesc.RTVFormats[i] = RTVFormats[i];
	for (UINT i = NumRTVs; i < m_PSODesc.NumRenderTargets; ++i)
		m_PSODesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
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
}
