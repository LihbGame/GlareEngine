#include "TemporalAA.h"
#include "Graphics/BufferManager.h"
#include "Graphics/GraphicsCore.h"
#include "Graphics/GraphicsCommon.h"
#include "Graphics/CommandContext.h"
#include "Engine/RenderMaterial.h"
#include "PostProcessing.h"


//shader
#include "CompiledShaders/TemporalAACS.h"
#include "CompiledShaders/ResolveTAACS.h"
#include "CompiledShaders/SharpenTAACS.h"
#include "MotionBlur.h"

namespace TemporalAA
{
	NumVar Sharpness(0.0f, 0.0f, 1.0f);

	//ue5 set 0.04 as default ,Low values cause blurriness and ghosting, high values fail to hide jittering
	float mCurrentFrameWeight = 0.04f;

	bool TriggerReset = false;

	RenderMaterial TemporalAAMaterial;
	RenderMaterial ResolveTAAMaterial;
	RenderMaterial SharpenTAAMaterial;

	uint32_t mFrameIndex = 0;
	uint32_t mFrameIndexMod2 = 0;
	XMFLOAT2 mJitter;

	float JitterDeltaX = 0.0f;
	float JitterDeltaY = 0.0f;

	void SharpenImage(ComputeContext& Context, ColorBuffer& TemporalColor);


	struct TAAConstant 
	{
		Matrix4 CurrentToPrev;
		XMFLOAT4 OutputViewSize;
		XMFLOAT4 InputViewSize;

		// Temporal jitter at the pixel.
		XMFLOAT2 TemporalJitterPixels;
		XMFLOAT2 ViewportJitter;

		float CurrentFrameWeight;
		float pad1;

		//CatmullRom Weight
		float SampleWeights[9];
		float PlusWeights[5];
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



void TemporalAA::ApplyTemporalAA(ComputeContext& Context,ColorBuffer& scenecolor)
{
	ScopedTimer TemporalAAScope(L"Temporal AA", Context);

	uint32_t Src = mFrameIndexMod2;
	uint32_t Dst = Src ^ 1;

	TAAConstant TemporalAAConstant;

	TemporalAAConstant.OutputViewSize = XMFLOAT4(g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight(),
		1.0f / g_SceneColorBuffer.GetWidth(), 1.0f / g_SceneColorBuffer.GetHeight());

	TemporalAAConstant.InputViewSize = TemporalAAConstant.OutputViewSize;
	TemporalAAConstant.TemporalJitterPixels = mJitter;
	TemporalAAConstant.CurrentFrameWeight = mCurrentFrameWeight;
	TemporalAAConstant.CurrentToPrev = MotionBlur::GetCurToPrevMatrix();
	TemporalAAConstant.ViewportJitter= XMFLOAT2(JitterDeltaX, JitterDeltaY);

	SetupSampleWeightParameters(TemporalAAConstant);

	Context.SetRootSignature(ScreenProcessing::GetRootSignature());

	Context.SetPipelineState(GET_PSO(TemporalAAMaterial.GetComputePSO()));

	Context.SetDynamicConstantBufferView(3, sizeof(TemporalAA::TAAConstant), &TemporalAAConstant);

	Context.SetDynamicDescriptor(1, 0, g_TemporalColor[Dst].GetUAV());

	Context.SetDynamicDescriptor(2, 0, g_LinearDepth[Src].GetSRV());
	Context.SetDynamicDescriptor(2, 1, g_LinearDepth[Dst].GetSRV());
	Context.SetDynamicDescriptor(2, 2, scenecolor.GetSRV());
	Context.SetDynamicDescriptor(2, 3, g_TemporalColor[Src].GetSRV());

	Context.SetDynamicDescriptor(2, 4, g_VelocityBuffer.GetSRV());

	Context.Dispatch2D(g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight());

	//Sharpen TAA
	SharpenImage(Context, g_TemporalColor[Dst]);
}


void TemporalAA::Initialize(void)
{
#define InitMaterial( MaterialName ,Material, ShaderByteCode ) \
    Material.BeginInitializeComputeMaterial(MaterialName,ScreenProcessing::GetRootSignature()); \
    Material.SetComputeShader(ShaderByteCode, sizeof(ShaderByteCode) ); \
	Material.EndInitializeComputeMaterial();

	InitMaterial(L"Temporal AA CS", TemporalAAMaterial, g_pTemporalAACS);
	InitMaterial(L"Sharpen TAA CS", SharpenTAAMaterial, g_pSharpenTAACS);
	InitMaterial(L"Resolve TAA CS", ResolveTAAMaterial, g_pResolveTAACS);

#undef InitMaterial

#if	USE_RUNTIME_PSO
	RuntimePSOManager::Get().RegisterPSO(&ResolveTAAMaterial.GetComputePSO(), GET_SHADER_PATH("PostProcessing/ResolveTAACS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&SharpenTAAMaterial.GetComputePSO(), GET_SHADER_PATH("PostProcessing/SharpenTAACS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&TemporalAAMaterial.GetComputePSO(), GET_SHADER_PATH("PostProcessing/TemporalAACS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
#endif


}

void TemporalAA::Shutdown(void)
{
}

/** [ Halton 1964, "Radical-inverse quasi-random point sequence" ] */
inline float Halton(int Index, int Base)
{
	float Result = 0.0f;
	float InvBase = 1.0f / float(Base);
	float Fraction = InvBase;
	while (Index > 0)
	{
		Result += float(Index % Base) * Fraction;
		Index /= Base;
		Fraction *= InvBase;
	}
	return Result;
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


	XMFLOAT2 halton = XMFLOAT2(Halton23[FrameIndex % 8][0], Halton23[FrameIndex % 8][1]);

	//// Scale distribution to set non-unit variance
	//float Sigma = 0.47f;

	//// Window to [-0.5, 0.5] output
	//// Without windowing we could generate samples far away on the infinite tails.
	//float OutWindow = 0.5f;
	//float InWindow = exp(-0.5 * sqrt(OutWindow / Sigma));

	//// Box-Muller transform
	//float Theta = 2.0f * MathHelper::Pi * halton.y;
	//float r = Sigma * Sqrt(-2.0f * logf((1.0f - halton.x) * InWindow + halton.x));

	//halton.x = r * cos(Theta);
	//halton.y = r * sin(Theta);

	JitterDeltaX = mJitter.x - halton.x;
	JitterDeltaY = mJitter.y - halton.y;

	mJitter.x = halton.x;
	mJitter.y = halton.y;
}

uint32_t TemporalAA::GetFrameIndexMod2(void)
{
	return mFrameIndexMod2;
}

XMFLOAT2 TemporalAA::GetJitterOffset()
{
	return XMFLOAT2(mJitter.x / (float)g_SceneColorBuffer.GetWidth(), mJitter.y / (float)g_SceneColorBuffer.GetHeight());
}

void TemporalAA::ClearHistory(CommandContext& Context)
{
	GraphicsContext& gfxContext = Context.GetGraphicsContext();
	gfxContext.TransitionResource(g_TemporalColor[0], D3D12_RESOURCE_STATE_RENDER_TARGET);
	gfxContext.TransitionResource(g_TemporalColor[1], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	gfxContext.ClearColor(g_TemporalColor[0]);
	gfxContext.ClearColor(g_TemporalColor[1]);
}


void TemporalAA::SharpenImage(ComputeContext& Context, ColorBuffer& TemporalColor)
{
	ScopedTimer SharpenScope(L"Sharpen or Copy Image", Context);

	Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(TemporalColor, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	Context.SetPipelineState(Sharpness >= 0.001f ? GET_PSO(SharpenTAAMaterial.GetComputePSO()) : GET_PSO(ResolveTAAMaterial.GetComputePSO()));
	Context.SetConstants(0, 1.0f + Sharpness, 0.25f * Sharpness);
	Context.SetDynamicDescriptor(2, 0, TemporalColor.GetSRV());
	Context.SetDynamicDescriptor(1, 0, g_SceneColorBuffer.GetUAV());
	Context.Dispatch2D(g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight());
}