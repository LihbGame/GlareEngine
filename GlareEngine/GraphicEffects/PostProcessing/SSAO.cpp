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

	NumVar SsaoRange(2, 1, 20);

	NumVar SsaoPower(6, 0.25f, 10);

	IntVar SsaoSampleCount(16, 1, 16);

	ColorBuffer* CurrentLinearDepth = nullptr;

	__declspec(align(16)) struct SSAORenderData
	{
		Matrix4			Proj;
		Matrix4			InvProj;
		float			Far;
		float			Near;
		float			Range;
		float			Power;
		XMFLOAT2		InverseDimensions;
		int				SampleCount;
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

	CurrentLinearDepth = &g_LinearDepth[FrameIndex];

	const float NearClipDist = camera.GetNearZ();

	const float FarClipDist = camera.GetFarZ();

	const float zMagic = (FarClipDist - NearClipDist) / NearClipDist;

	LinearizeZ(Context, Depth, CurrentLinearDepth, zMagic);
}

void SSAO::LinearizeZ(ComputeContext& Context, DepthBuffer& Depth, ColorBuffer* linearDepth, float zMagic)
{
	assert(linearDepth);

	ColorBuffer& LinearDepth = *linearDepth;

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
	if (Enable)
	{
		assert(CurrentLinearDepth);

		// Flush the PrePass and wait for it on the compute queue
		g_CommandManager.GetComputeQueue().StallForFence(Context.Flush());

		Context.TransitionResource(*CurrentLinearDepth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		Context.TransitionResource(g_SSAOFullScreen, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		ComputeContext& computeContext = ComputeContext::Begin(L"SSAO", true);

		computeContext.SetRootSignature(ScreenProcessing::GetRootSignature());

		SSAORenderData renderData{
			Matrix4(RenderData.Proj),
			Matrix4(RenderData.InvProj),
			RenderData.FarZ,RenderData.NearZ,
			SsaoRange,SsaoPower,XMFLOAT2(1.0f / g_SSAOFullScreen.GetWidth(),1.0f / g_SSAOFullScreen.GetHeight()),SsaoSampleCount };

		computeContext.SetDynamicConstantBufferView(3, sizeof(SSAORenderData), &renderData);

		computeContext.SetDynamicDescriptor(1, 0, g_SSAOFullScreen.GetUAV());

		D3D12_CPU_DESCRIPTOR_HANDLE Depth[2] = { g_SceneDepthBuffer.GetDepthSRV(),CurrentLinearDepth->GetSRV() };

		computeContext.SetDynamicDescriptors(2, 0, 2, Depth);

		computeContext.SetPipelineState(SsaoCS);

		computeContext.Dispatch2D(g_SSAOFullScreen.GetWidth(), g_SSAOFullScreen.GetHeight());



		computeContext.Finish();

	}
	else
	{
		Context.ClearColor(g_SSAOFullScreen);
	}

#ifdef DEBUG
	EngineGUI::AddRenderPassVisualizeTexture("SSAO", WStringToString(g_SSAOFullScreen.GetName()), g_SSAOFullScreen.GetHeight(), g_SSAOFullScreen.GetWidth(), g_SSAOFullScreen.GetSRV());
#endif
}

void SSAO::DrawUI()
{
	ImGui::Checkbox("Enable SSAO", &Enable);
	if (Enable)
	{
		ImGui::SliderVerticalFloat("Range", &SsaoRange.GetValue(), SsaoRange.GetMinValue(), SsaoRange.GetMaxValue());
		ImGui::SliderVerticalFloat("Power", &SsaoPower.GetValue(), SsaoPower.GetMinValue(), SsaoPower.GetMaxValue());
		ImGui::SliderVerticalInt("Sample Count", &SsaoSampleCount.GetValue(), SsaoSampleCount.GetMinValue(), SsaoSampleCount.GetMaxValue());
	}
}



