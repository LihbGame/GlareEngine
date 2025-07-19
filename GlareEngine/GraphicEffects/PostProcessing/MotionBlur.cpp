#include "MotionBlur.h"
#include "Graphics/PipelineState.h"
#include "PostProcessing.h"
#include "Graphics/CommandContext.h"
#include "Engine/EngineProfiling.h"
#include "EngineGUI.h"

//shader
#include "CompiledShaders/MotionBlurPrePassCS.h"
#include "CompiledShaders/MotionBlurFinalPassCS.h"
#include "CompiledShaders/CameraVelocityCS.h"
#include "TemporalAA.h"


namespace MotionBlur
{
	bool IsEnable = true;

	NumVar MaxSampleCount(10, 10, 100);
	NumVar StepSize(1.0f, 0.01f, 3.0f);

	ComputePSO MotionBlurPrePassCS(L"Motion Blur PrePass CS");
	ComputePSO MotionBlurFinalPassCS(L"Motion Blur Final Pass CS");
	ComputePSO CameraVelocityCS = { L"Camera Velocity Linear Z CS" };

	Matrix4 gCurToPrevMatrix;
};


void MotionBlur::Initialize(void)
{
#define CreatePSO( ObjName, ShaderByteCode ) \
    ObjName.SetRootSignature(ScreenProcessing::GetRootSignature()); \
    ObjName.SetComputeShader(ShaderByteCode, sizeof(ShaderByteCode) ); \
    ObjName.Finalize();

	CreatePSO(MotionBlurFinalPassCS, g_pMotionBlurFinalPassCS);
	CreatePSO(MotionBlurPrePassCS, g_pMotionBlurPrePassCS);
	CreatePSO(CameraVelocityCS, g_pCameraVelocityCS);

#undef CreatePSO
}

void MotionBlur::Shutdown(void)
{

}

void MotionBlur::GenerateCameraVelocityBuffer(CommandContext& BaseContext, const Camera& camera, bool UseLinearZ)
{
	ScopedTimer CameraVelocityScope(L"Generate Camera Velocity", BaseContext);

	ComputeContext& Context = BaseContext.GetComputeContext();

	Context.SetRootSignature(ScreenProcessing::GetRootSignature());

	uint32_t Width = g_SceneColorBuffer.GetWidth();
	uint32_t Height = g_SceneColorBuffer.GetHeight();

	float RcpHalfDimX = 2.0f / Width;
	float RcpHalfDimY = 2.0f / Height;

	float RcpZMagic = camera.GetNearZ() / (camera.GetFarZ() - camera.GetNearZ());

	//Screen To NDC
	Matrix4 preMult = Matrix4(
		Vector4(RcpHalfDimX, 0.0f, 0.0f, 0.0f),
		Vector4(0.0f, -RcpHalfDimY, 0.0f, 0.0f),
		Vector4(0.0f, 0.0f, UseLinearZ ? RcpZMagic : 1.0f, 0.0f),
		Vector4(-1.0f, 1.0f, UseLinearZ ? -RcpZMagic : 0.0f, 1.0f)
	);

	//NDC To Screen
	Matrix4 postMult = Matrix4(
		Vector4(1.0f / RcpHalfDimX, 0.0f, 0.0f, 0.0f),
		Vector4(0.0f, -1.0f / RcpHalfDimY, 0.0f, 0.0f),
		Vector4(0.0f, 0.0f, 1.0f, 0.0f),
		Vector4(1.0f / RcpHalfDimX, 1.0f / RcpHalfDimY, 0.0f, 1.0f));


	gCurToPrevMatrix = preMult * camera.GetReprojectMatrix() * postMult;

	Context.SetDynamicConstantBufferView(3, sizeof(gCurToPrevMatrix), &gCurToPrevMatrix);

	Context.TransitionResource(g_VelocityBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	ColorBuffer& LinearDepth = g_LinearDepth[TemporalAA::GetFrameIndexMod2()];

	if (UseLinearZ)
		Context.TransitionResource(LinearDepth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	else
		Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	Context.SetPipelineState(CameraVelocityCS);

	Context.SetDynamicDescriptor(1, 0, g_VelocityBuffer.GetUAV());
	Context.SetDynamicDescriptor(2, 0, UseLinearZ ? LinearDepth.GetSRV() : g_SceneDepthBuffer.GetDepthSRV());
	Context.Dispatch2D(Width, Height);
}

void MotionBlur::RenderMotionBlur(CommandContext& BaseContext, ColorBuffer& velocityBuffer, ColorBuffer* Input)
{
	if (!IsEnable)
		return;

	ScopedTimer MotionBlurScope(L"MotionBlur", BaseContext);

	uint32_t Width = Input->GetWidth();
	uint32_t Height = Input->GetHeight();

	ComputeContext& Context = BaseContext.GetComputeContext();

	Context.SetRootSignature(ScreenProcessing::GetRootSignature());

	Context.TransitionResource(g_MotionPrepBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(*Input, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(velocityBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	Context.SetDynamicDescriptor(1, 0, g_MotionPrepBuffer.GetUAV());
	Context.SetDynamicDescriptor(2, 0, Input->GetSRV());
	Context.SetDynamicDescriptor(2, 1, velocityBuffer.GetSRV());

	Context.SetPipelineState(MotionBlurPrePassCS);
	Context.Dispatch2D(g_MotionPrepBuffer.GetWidth(), g_MotionPrepBuffer.GetHeight());

	Context.SetPipelineState(MotionBlurFinalPassCS);

	Context.TransitionResource(*Input, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(velocityBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(g_MotionPrepBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	Context.SetDynamicDescriptor(1, 0, Input->GetUAV());
	Context.SetDynamicDescriptor(2, 0, velocityBuffer.GetSRV());
	Context.SetDynamicDescriptor(2, 1, g_MotionPrepBuffer.GetSRV());
	Context.SetConstants(0, 1.0f / Width, 1.0f / Height, MaxSampleCount.GetValue(), StepSize.GetValue());

	Context.Dispatch2D(Width, Height);

	//Context.InsertUAVBarrier(g_SceneColorBuffer);
}

void MotionBlur::DrawUI()
{
	ImGui::Checkbox("Enable MotionBlur", &IsEnable);
	if (IsEnable)
	{
		ImGui::SliderVerticalFloat("Max Sample Count", &MaxSampleCount.GetValue(), MaxSampleCount.GetMinValue(), MaxSampleCount.GetMaxValue());
		ImGui::SliderVerticalFloat("Step Size", &StepSize.GetValue(), StepSize.GetMinValue(), StepSize.GetMaxValue());
	}
}

const Matrix4& MotionBlur::GetCurToPrevMatrix()
{
	return gCurToPrevMatrix;
}
