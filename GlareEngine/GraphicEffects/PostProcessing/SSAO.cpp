#include "SSAO.h"
#include "Graphics/SamplerManager.h"
#include "PostProcessing/PostProcessing.h"
#include "Engine/EngineProfiling.h"
#include "EngineGUI.h"
#include "Engine/Tools/RuntimePSOManager.h"
//shader
#include "CompiledShaders/SsaoCS.h"


namespace SSAO
{
	bool Enable = true;

	ComputePSO SsaoCS(L"SSAO CS");

	NumVar SsaoRange(2, 1, 20);

	NumVar SsaoPower(6, 0.25f, 10);

	IntVar SsaoSampleCount(16, 1, 16);

	bool IsWideBlur = false;

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

#if	USE_RUNTIME_PSO
	std::map<PSO*, std::string> PSOMap;
#endif
}


void SSAO::Initialize(void)
{
	//Create Compute PSO Macro
#define CreatePSO( ObjName, ShaderByteCode ) \
    ObjName.SetRootSignature(ScreenProcessing::GetRootSignature()); \
    ObjName.SetComputeShader(ShaderByteCode, sizeof(ShaderByteCode) ); \
    ObjName.Finalize();

	CreatePSO(SsaoCS, g_pSsaoCS);

#undef CreatePSO

#if	USE_RUNTIME_PSO
	PSOMap[&SsaoCS] = std::string(SHADER_ASSET_DIRECTOTY) + "PostProcessing/SsaoCS.hlsl";
	RuntimePSOManager::Get().RegisterPSO(&SsaoCS, PSOMap[&SsaoCS].c_str(), D3D12_SHVER_COMPUTE_SHADER);
#endif
}

void SSAO::Shutdown(void)
{
}

void SSAO::Render(GraphicsContext& Context, MainConstants& RenderData)
{
	if (Enable)
	{
		// Flush the PrePass and wait for it on the compute queue
		//g_CommandManager.GetComputeQueue().StallForFence(Context.Flush());

		Context.TransitionResource(*ScreenProcessing::GetLinearDepthBuffer(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		Context.TransitionResource(g_SSAOFullScreen, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		//ComputeContext& computeContext = ComputeContext::Begin(L"SSAO", true);

		ComputeContext& computeContext = Context.GetComputeContext();

		ScopedTimer SSAOScope(L"SSAO", Context);

		computeContext.SetRootSignature(ScreenProcessing::GetRootSignature());

		SSAORenderData renderData{
			Matrix4(RenderData.Proj),
			Matrix4(RenderData.InvProj),
			RenderData.FarZ,RenderData.NearZ,
			SsaoRange,SsaoPower,XMFLOAT2(1.0f / g_SSAOFullScreen.GetWidth(),1.0f / g_SSAOFullScreen.GetHeight()),SsaoSampleCount };

		computeContext.SetDynamicConstantBufferView(3, sizeof(SSAORenderData), &renderData);

		computeContext.SetDynamicDescriptor(1, 0, g_SSAOFullScreen.GetUAV());

		D3D12_CPU_DESCRIPTOR_HANDLE Depth[2] = { g_SceneDepthBuffer.GetDepthSRV(),ScreenProcessing::GetLinearDepthBuffer()->GetSRV() };

		computeContext.SetDynamicDescriptors(2, 0, 2, Depth);

		computeContext.SetPipelineState(GET_PSO(SsaoCS));

		computeContext.Dispatch2D(g_SSAOFullScreen.GetWidth(), g_SSAOFullScreen.GetHeight());

		ScreenProcessing::BilateralBlur(computeContext, g_SSAOFullScreen, g_BlurTemp_HalfBuffer_R8, IsWideBlur);
	}
	else
	{
		Context.ClearUAV(g_SSAOFullScreen);
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
		ImGui::Checkbox("Wide Blur", &IsWideBlur);
		ImGui::SliderVerticalFloat("Range", &SsaoRange.GetValue(), SsaoRange.GetMinValue(), SsaoRange.GetMaxValue());
		ImGui::SliderVerticalFloat("Power", &SsaoPower.GetValue(), SsaoPower.GetMinValue(), SsaoPower.GetMaxValue());
		ImGui::SliderVerticalInt("Sample Count", &SsaoSampleCount.GetValue(), SsaoSampleCount.GetMinValue(), SsaoSampleCount.GetMaxValue());
	}
}



