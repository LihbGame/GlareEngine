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

	//ue5 set 0.04 as default ,Low values cause blurriness and ghosting, high values fail to hide jittering
	float mCurrentFrameWeight = 0.04f;

	bool TriggerReset = false;

	ComputePSO TemporalAACS(L"Temporal AA CS");

	uint32_t mFrameIndex = 0;
	uint32_t mFrameIndexMod2 = 0;
	XMFLOAT2 mJitter;

	void ApplyTemporalAA(ComputeContext& Context);
	void SharpenImage(ComputeContext& Context, ColorBuffer& TemporalColor);


	struct TAAConstant 
	{
		XMFLOAT4 OutputViewSize;
		XMFLOAT4 InputViewSize;

		// Temporal jitter at the pixel.
		XMFLOAT2 TemporalJitterPixels;

		//CatmullRom Weight
		float SampleWeights[9];
		float PlusWeights[5];
		float CurrentFrameWeight;
	};
};

float CatmullRom(float x)
{
	float ax = abs(x);
	if (ax > 1.0f)
		return ((-0.5f * ax + 2.5f) * ax - 4.0f) * ax + 2.0f;
	else
		return (1.5f * ax - 2.5f) * ax * ax + 1.0f;
}

void SetupSampleWeightParameters(TemporalAA::TAAConstant& taaConstant)
{
	float JitterX = taaConstant.TemporalJitterPixels.x;
	float JitterY = taaConstant.TemporalJitterPixels.y;

	static const float SampleOffsets[9][2] =
	{
		{ -1.0f, -1.0f },
		{  0.0f, -1.0f },
		{  1.0f, -1.0f },
		{ -1.0f,  0.0f },
		{  0.0f,  0.0f },
		{  1.0f,  0.0f },
		{ -1.0f,  1.0f },
		{  0.0f,  1.0f },
		{  1.0f,  1.0f },
	};

	// Compute 3x3 weights
	{
		float TotalWeight = 0.0f;
		for (int32_t i = 0; i < 9; i++)
		{
			float PixelOffsetX = SampleOffsets[i][0] - JitterX;
			float PixelOffsetY = SampleOffsets[i][1] - JitterY;

			const float CurrWeight = CatmullRom(PixelOffsetX) * CatmullRom(PixelOffsetY);
			taaConstant.SampleWeights[i] = CurrWeight;
			TotalWeight += CurrWeight;

		}

		for (int32_t i = 0; i < 9; i++)
		{
			taaConstant.SampleWeights[i] /= TotalWeight;
		}
	}

	// Compute 3x3 + weights.
	{
		taaConstant.PlusWeights[0] = taaConstant.SampleWeights[1];
		taaConstant.PlusWeights[1] = taaConstant.SampleWeights[3];
		taaConstant.PlusWeights[2] = taaConstant.SampleWeights[4];
		taaConstant.PlusWeights[3] = taaConstant.SampleWeights[5];
		taaConstant.PlusWeights[4] = taaConstant.SampleWeights[7];

		float TotalWeightPlus = taaConstant.SampleWeights[1] +
								taaConstant.SampleWeights[3] +
								taaConstant.SampleWeights[4] +
								taaConstant.SampleWeights[5] +
								taaConstant.SampleWeights[7];

		for (int32_t i = 0; i < 5; i++)
		{
			taaConstant.PlusWeights[i] /= TotalWeightPlus;
		}
	}
}



void TemporalAA::ApplyTemporalAA(ComputeContext& Context)
{
	ScopedTimer TemporalAAScope(L"Temporal AA", Context);

	uint32_t Src = mFrameIndexMod2;
	uint32_t Dst = Src ^ 1;

	TAAConstant TemporalAAConstant;

	TemporalAAConstant.OutputViewSize = XMFLOAT4(g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight(),
		1.0f / g_SceneColorBuffer.GetWidth(), 1.0f / g_SceneColorBuffer.GetHeight());

	TemporalAAConstant.InputViewSize = TemporalAAConstant.OutputViewSize;
	TemporalAAConstant.TemporalJitterPixels = XMFLOAT2(mJitter.x * g_SceneColorBuffer.GetWidth(), mJitter.y * g_SceneColorBuffer.GetHeight());
	TemporalAAConstant.CurrentFrameWeight = mCurrentFrameWeight;

	SetupSampleWeightParameters(TemporalAAConstant);

	Context.SetRootSignature(ScreenProcessing::GetRootSignature());

	Context.SetPipelineState(TemporalAACS);

	Context.SetDynamicConstantBufferView(0, sizeof(TemporalAA::Sharpness), &TemporalAA::Sharpness);

	Context.SetDynamicDescriptor(1, 0, g_SceneColorBuffer.GetSRV());
	Context.SetDynamicDescriptor(1, 1, g_TemporalColor[Src].GetSRV());
	Context.SetDynamicDescriptor(1, 2, g_LinearDepth.GetSRV());

	Context.SetDynamicDescriptor(2, 0, g_TemporalColor[Dst].GetUAV());

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

	const XMFLOAT4& halton = MathHelper::GetHaltonSequence(mFrameIndex % 256);
	mJitter.x = (halton.x * 2 - 1) / (float)g_SceneColorBuffer.GetWidth();
	mJitter.y = (halton.y * 2 - 1) / (float)g_SceneColorBuffer.GetHeight();
}

uint32_t TemporalAA::GetFrameIndexMod2(void)
{
	return mFrameIndexMod2;
}

XMFLOAT2& TemporalAA::GetJitterOffset()
{
	return mJitter;
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
