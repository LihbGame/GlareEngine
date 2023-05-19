#include "SSAO.h"
#include "Graphics/SamplerManager.h"
#include "PostProcessing/PostProcessing.h"
#include "EngineGUI.h"

//shader
#include "CompiledShaders/LinearizeDepthCS.h"
#include "CompiledShaders/SsaoCS.h"

namespace SSAO
{
	bool Enable = true;

	ComputePSO LinearizeDepthCS(L"Linearize Depth CS");
	ComputePSO SsaoCS(L"SSAO CS");

	struct SSAORenderData
	{
		Matrix4 Proj;
		Matrix4 InvProj;
		float Far;
		float Near;
	};
}


void SSAO::Initialize(void)
{
	//Create Compute PSO Macro
#define CreatePSO( ObjName, ShaderByteCode ) \
    ObjName.SetRootSignature(ScreenProcessing::GetRootSignature()); \
    ObjName.SetComputeShader(ShaderByteCode, sizeof(ShaderByteCode) ); \
    ObjName.Finalize();

	CreatePSO(LinearizeDepthCS, g_pLinearizeDepthCS);

	CreatePSO(SsaoCS, g_pSsaoCS);

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
	Context.SetRootSignature(ScreenProcessing::GetRootSignature());

	Context.TransitionResource(Depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	Context.SetConstants(0, zMagic);

	Context.SetDynamicDescriptor(2, 0, Depth.GetDepthSRV());

	Context.TransitionResource(LinearDepth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	Context.SetDynamicDescriptors(1, 0, 1, &LinearDepth.GetUAV());

	Context.SetPipelineState(LinearizeDepthCS);

	Context.Dispatch2D(LinearDepth.GetWidth(), LinearDepth.GetHeight(), 16, 16);

#ifdef DEBUG
	EngineGUI::AddRenderPassVisualizeTexture("Linear Z", WStringToString(LinearDepth.GetName()), LinearDepth.GetHeight(), LinearDepth.GetWidth(), LinearDepth.GetSRV()); 
#endif // DEBUG
}

void SSAO::Render(GraphicsContext& Context, MainConstants& RenderData)
{
	// Flush the PrePass and wait for it on the compute queue
	g_CommandManager.GetComputeQueue().StallForFence(Context.Flush());

	Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	Context.TransitionResource(g_SSAOFullScreen, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	ComputeContext& computeContext = ComputeContext::Begin(L"SSAO", true);

	computeContext.SetRootSignature(ScreenProcessing::GetRootSignature());

	SSAORenderData renderData{ Matrix4(RenderData.Proj),Matrix4(RenderData.InvProj), RenderData.FarZ,RenderData.NearZ };

	computeContext.SetDynamicConstantBufferView(3, sizeof(SSAORenderData), &renderData);

	computeContext.SetDynamicDescriptor(1, 0, g_SSAOFullScreen.GetUAV());

	computeContext.SetDynamicDescriptor(2, 0, g_SceneDepthBuffer.GetDepthSRV());

	computeContext.SetPipelineState(SsaoCS);

	computeContext.Dispatch2D(g_SSAOFullScreen.GetWidth(), g_SSAOFullScreen.GetHeight());
}


