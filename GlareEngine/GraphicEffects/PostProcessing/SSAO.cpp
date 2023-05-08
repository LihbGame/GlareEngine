#include "SSAO.h"
#include "Graphics/SamplerManager.h"

namespace SSAO
{
	bool Enable = true;
	bool AsyncCompute = false;
	bool ComputeLinearZ = true;

	RootSignature s_RootSignature;
}


void SSAO::Initialize(void)
{
	s_RootSignature.Reset(5, 2);
	s_RootSignature.InitStaticSampler(0, SamplerLinearClampDesc);
	s_RootSignature.InitStaticSampler(1, SamplerLinearBorderDesc);
	s_RootSignature[0].InitAsConstants(0, 4);
	s_RootSignature[1].InitAsConstantBuffer(1);
	s_RootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 5);
	s_RootSignature[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 5);
	s_RootSignature[4].InitAsBufferSRV(5);
	s_RootSignature.Finalize(L"SSAO");

}

void SSAO::Shutdown(void)
{
}

void SSAO::Render(GraphicsContext& Context, const float* ProjMat, float NearClip, float FarClip)
{

}

void SSAO::Render(GraphicsContext& Context, Camera& camera)
{
}


