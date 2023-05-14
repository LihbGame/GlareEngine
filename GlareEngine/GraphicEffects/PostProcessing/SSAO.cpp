#include "SSAO.h"
#include "Graphics/SamplerManager.h"
#include "EngineGUI.h"

//shader
#include "CompiledShaders/LinearizeDepthCS.h"

namespace SSAO
{
	bool Enable = true;
	bool AsyncCompute = false;
	bool ComputeLinearZ = true;

	RootSignature s_RootSignature;

	ComputePSO LinearizeDepthCS(L"Linearize Depth CS");

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

	//Create Compute PSO Macro
#define CreatePSO( ObjName, ShaderByteCode ) \
    ObjName.SetRootSignature(s_RootSignature); \
    ObjName.SetComputeShader(ShaderByteCode, sizeof(ShaderByteCode) ); \
    ObjName.Finalize();

	CreatePSO(LinearizeDepthCS, g_pLinearizeDepthCS);

#undef CreatePSO
}

void SSAO::Shutdown(void)
{
}

void SSAO::LinearizeZ(ComputeContext& Context, Camera& camera, uint32_t FrameIndex)
{
	DepthBuffer& Depth = g_SceneDepthBuffer;
	ColorBuffer& LinearDepth = g_LinearDepth[FrameIndex];
	const float NearClipDist = camera.GetNearZ();
	const float FarClipDist = camera.GetFarZ();
	const float zMagic = (FarClipDist - NearClipDist) / NearClipDist;

	LinearizeZ(Context, Depth, LinearDepth, zMagic);
}

void SSAO::LinearizeZ(ComputeContext& Context, DepthBuffer& Depth, ColorBuffer& LinearDepth, float zMagic)
{
	// zMagic= (zFar - zNear) / zNear
	Context.SetRootSignature(s_RootSignature);
	Context.TransitionResource(Depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.SetConstants(0, zMagic);
	Context.SetDynamicDescriptor(3, 0, Depth.GetDepthSRV());
	Context.TransitionResource(LinearDepth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.SetDynamicDescriptors(2, 0, 1, &LinearDepth.GetUAV());
	Context.SetPipelineState(LinearizeDepthCS);
	Context.Dispatch2D(LinearDepth.GetWidth(), LinearDepth.GetHeight(), 16, 16);

#ifdef DEBUG
	EngineGUI::AddRenderPassVisualizeTexture("Linear Z", WStringToString(LinearDepth.GetName()), LinearDepth.GetHeight(), LinearDepth.GetWidth(), LinearDepth.GetSRV()); 
#endif // DEBUG
}

void SSAO::Render(GraphicsContext& Context, const Matrix4& ProjMat)
{
	const float* pProjMat = reinterpret_cast<const float*>(&ProjMat);

	const float FovTangent = 1.0f / pProjMat[0];

	// Flush the PrePass and wait for it on the compute queue
	g_CommandManager.GetComputeQueue().StallForFence(Context.Flush());

	Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	Context.TransitionResource(g_SSAOFullScreen, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	ComputeContext& computeContext = ComputeContext::Begin(L"Async SSAO", true);

	computeContext.SetRootSignature(s_RootSignature);


}


