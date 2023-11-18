#include "TemporalAA.h"
#include "Graphics/BufferManager.h"
#include "Graphics/GraphicsCore.h"
#include "Graphics/GraphicsCommon.h"
#include "Graphics/CommandContext.h"
#include "PostProcessing.h"


//shader
#include "CompiledShaders/TemporalAACS.h"

namespace TemporalAA
{
	NumVar Sharpness(0.5f, 0.0f, 1.0f);
	bool TriggerReset = false;

	ComputePSO TemporalAACS(L"Temporal AA CS");

	uint32_t mFrameIndex = 0;
	uint32_t mFrameIndexMod2 = 0;
	float mJitterX = 0.5f;
	float mJitterY = 0.5f;
	float mJitterDeltaX = 0.0f;
	float mJitterDeltaY = 0.0f;

	void ApplyTemporalAA(ComputeContext& Context);
	void SharpenImage(ComputeContext& Context, ColorBuffer& TemporalColor);

};

void TemporalAA::ApplyTemporalAA(ComputeContext& Context)
{
	ScopedTimer TemporalAAScope(L"Temporal AA", Context);

	Context.SetRootSignature(ScreenProcessing::GetRootSignature());

	Context.SetPipelineState(TemporalAACS);

	Context.SetDynamicConstantBufferView(0, sizeof(TemporalAA::Sharpness), &TemporalAA::Sharpness);

	Context.SetDynamicDescriptor(1, 0, g_SceneColorBuffer.GetSRV());
	Context.SetDynamicDescriptor(1, 1, g_TemporalColor[0].GetSRV());
	Context.SetDynamicDescriptor(1, 2, g_LinearDepth.GetSRV());

	Context.SetDynamicDescriptor(2, 0, g_TemporalColor[1].GetUAV());

	Context.Dispatch2D(g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight());
}

void TemporalAA::SharpenImage(ComputeContext& Context, ColorBuffer& TemporalColor)
{
}

void TemporalAA::Initialize(void)
{
#define CreatePSO( ObjName, ShaderByteCode ) \
    ObjName.SetRootSignature(ScreenProcessing::GetRootSignature()); \
    ObjName.SetComputeShader(ShaderByteCode, sizeof(ShaderByteCode) ); \
    ObjName.Finalize();

	CreatePSO(TemporalAACS, g_pTemporalAACS);

#undef CreatePSO

}

void TemporalAA::Shutdown(void)
{
}

void TemporalAA::Update(uint64_t FrameIndex)
{
	mFrameIndex = (uint32_t)FrameIndex;
	mFrameIndexMod2 = mFrameIndex % 2;

	static const float Halton23[8][2] =
	{
		{ 0.0f / 8.0f, 0.0f / 9.0f }, { 4.0f / 8.0f, 3.0f / 9.0f },
		{ 2.0f / 8.0f, 6.0f / 9.0f }, { 6.0f / 8.0f, 1.0f / 9.0f },
		{ 1.0f / 8.0f, 4.0f / 9.0f }, { 5.0f / 8.0f, 7.0f / 9.0f },
		{ 3.0f / 8.0f, 2.0f / 9.0f }, { 7.0f / 8.0f, 5.0f / 9.0f }
	};

	const float* Offset = nullptr;
	Offset = Halton23[FrameIndex % 8];

	mJitterDeltaX = mJitterX - Offset[0];
	mJitterDeltaY = mJitterY - Offset[1];
	mJitterX = Offset[0];
	mJitterY = Offset[1];
}

uint32_t TemporalAA::GetFrameIndexMod2(void)
{
	return mFrameIndexMod2;
}

void TemporalAA::GetJitterOffset(float& jitterX, float& jitterY)
{
	jitterX = mJitterX;
	jitterY = mJitterY;
}

void TemporalAA::ClearHistory(CommandContext& Context)
{
	GraphicsContext& gfxContext = Context.GetGraphicsContext();
	gfxContext.TransitionResource(g_TemporalColor[0], D3D12_RESOURCE_STATE_RENDER_TARGET);
	gfxContext.TransitionResource(g_TemporalColor[1], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	gfxContext.ClearColor(g_TemporalColor[0]);
	gfxContext.ClearColor(g_TemporalColor[1]);
}

void TemporalAA::ResolveImage(CommandContext& Context)
{
}
