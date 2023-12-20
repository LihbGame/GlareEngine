#include "FXAA.h"
#include "Engine/EngineProfiling.h"
#include "EngineGUI.h"
#include "PostProcessing/PostProcessing.h"

//shader
#include "CompiledShaders/FXAACS.h"

namespace FXAA
{
	bool IsEnable = true;

	//[0.25 = "low", 0.2 = "medium", 0.15 = "high", 0.1 = "ultra"]
	NumVar ContrastThreshold(0.175f, 0.05f, 0.5f, 0.025f);

	// Controls how much to blur isolated pixels that have little-to-no edge length.
	NumVar SubpixelRemoval(0.50f, 0.0f, 1.0f, 0.25f);

	ComputePSO FxaaCS(L"FXAA");

	__declspec(align(16)) struct FXAARenderData
	{
		XMFLOAT2		InverseDimensions;
	};

};

void FXAA::Initialize(void)
{
#define CreatePSO( ObjName, ShaderByteCode ) \
    ObjName.SetRootSignature(ScreenProcessing::GetRootSignature()); \
    ObjName.SetComputeShader(ShaderByteCode, sizeof(ShaderByteCode) ); \
    ObjName.Finalize();

	CreatePSO(FxaaCS, g_pFXAACS);

#undef CreatePSO
}


void FXAA::Render(ComputeContext& Context, ColorBuffer* Input, ColorBuffer* Output)
{

	ScopedTimer FXAAScope(L"FXAA", Context);
	Context.TransitionResource(*Output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	Context.SetRootSignature(ScreenProcessing::GetRootSignature());

	FXAARenderData renderData{ XMFLOAT2(1.0f / Output->GetWidth(),1.0f / Output->GetHeight()) };

	Context.SetDynamicConstantBufferView(3, sizeof(FXAARenderData), &renderData);

	Context.SetDynamicDescriptor(1, 0, Output->GetUAV());

	Context.SetDynamicDescriptor(2, 0, Input->GetSRV());

	Context.SetPipelineState(FxaaCS);

	Context.Dispatch2D(Output->GetWidth(), Output->GetHeight());
}

void FXAA::DrawUI()
{
	ImGui::Checkbox("Enable FXAA", &IsEnable);
}
