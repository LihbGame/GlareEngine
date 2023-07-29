#include "MotionBlur.h"
#include "Graphics/PipelineState.h"
#include "PostProcessing.h"
#include "Graphics/CommandContext.h"
#include "Engine/EngineProfiling.h"

//shader
#include "CompiledShaders/MotionBlurPrePassCS.h"
#include "CompiledShaders/MotionBlurFinalPassCS.h"
#include "CompiledShaders/CameraVelocityCS.h"


namespace MotionBlur
{
	bool IsEnable = false;

	ComputePSO MotionBlurPrePassCS(L"Motion Blur PrePass CS");
	ComputePSO MotionBlurFinalPassCS(L"Motion Blur Final Pass CS");
	ComputePSO CameraVelocityCS = { L"Camera Velocity Linear Z CS" };
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
	ScopedTimer Marker(L"Generate Camera Velocity", BaseContext);

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


	Matrix4 CurToPrevMatrix = preMult * camera.GetReprojectMatrix() * postMult;

	Context.SetDynamicConstantBufferView(3, sizeof(CurToPrevMatrix), &CurToPrevMatrix);

	Context.TransitionResource(g_VelocityBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	//TemporalEffects::GetFrameIndexMod2() will be change when implement taa
	ColorBuffer& LinearDepth = g_LinearDepth[0];

	if (UseLinearZ)
		Context.TransitionResource(LinearDepth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	else
		Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	Context.SetPipelineState(CameraVelocityCS);
	Context.SetDynamicDescriptor(1, 0, UseLinearZ ? LinearDepth.GetSRV() : g_SceneDepthBuffer.GetDepthSRV());
	Context.SetDynamicDescriptor(2, 0, g_VelocityBuffer.GetUAV());
	Context.Dispatch2D(Width, Height);
}
