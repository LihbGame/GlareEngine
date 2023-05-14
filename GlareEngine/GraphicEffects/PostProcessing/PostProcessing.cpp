#include "Engine/EngineProfiling.h"
#include "PostProcessing.h"
#include "Graphics/GraphicsCommon.h"
#include "Graphics/SamplerManager.h"
#include "Graphics/CommandSignature.h"
#include "EngineGUI.h"
#include "Graphics/Display.h"

//shaders
#include "CompiledShaders/ScreenQuadVS.h"
#include "CompiledShaders/FbmPostPS.h"
#include "CompiledShaders/ApplyBloomCS.h"
#include "CompiledShaders/ApplyBloom2CS.h"
#include "CompiledShaders/BloomDownSample2CS.h"
#include "CompiledShaders/BloomDownSample4CS.h"
#include "CompiledShaders/BloomExtractAndDownSampleHDRCS.h"
#include "CompiledShaders/UpsampleBlurCS.h"
#include "CompiledShaders/BlurCS.h"
#include "CompiledShaders/ToneMapCS.h"
#include "CompiledShaders/ToneMap2CS.h"
#include "CompiledShaders/ToneMapHDRCS.h"
#include "CompiledShaders/ToneMapHDR2CS.h"
#include "CompiledShaders/ExtractLuminanceCS.h"
#include "CompiledShaders/CopyPostBufferHDRCS.h"
#include "CompiledShaders/GenerateLuminanceHistogramCS.h"
#include "CompiledShaders/AdaptExposureCS.h"
#include "CompiledShaders/DrawHistogramCS.h"

namespace ScreenProcessing
{

	__declspec(align(16)) struct AdaptationConstants
	{
		float TargetLuminance;
		float AdaptationRate;
		float MinExposure;
		float MaxExposure;
		uint32_t PixelCount;
	};


	bool  BloomEnable = true;
	bool  HighQualityBloom = true;					// High quality blurs 5 octaves of bloom; low quality only blurs 3.
	NumVar BloomThreshold(4.0f, 0.0f, 8.0f);		// The threshold luminance above which a pixel will start to bloom
	NumVar BloomStrength(0.1f, 0.0f, 2.0f);			// A modulator controlling how much bloom is added back into the image
	NumVar BloomUpSampleFactor(0.65f, 0.0f, 1.0f);	// Controls the "focus" of the blur.High values spread out more causing a haze.

	//Exposure Log() Range
	const float InitialMinLog = -12.0f;
	const float InitialMaxLog = 4.0f;

	bool EnableAdaptation = true;
	bool DrawHistogram = false;

	NumVar TargetLuminance(0.05f, 0.01f, 0.99f);
	NumVar AdaptationTranform(0.02f, 0.01f, 1.0f);
	NumVar Exposure(2.0f, -8.0f, 8.0f);
	
	float MinExposure = 1.0f / 64.0f;
	float MaxExposure = 64.0f;

	//Exposure Data
	StructuredBuffer g_Exposure;

	RootSignature PostEffectsRS;
	GraphicsPSO mPSO(L"FBM Post PS");

	//Bloom
	ComputePSO DownsampleBloom2CS(L"DownSample Bloom 2 CS");
	ComputePSO DownsampleBloom4CS(L"DownSample Bloom 4 CS");
	ComputePSO BloomExtractAndDownsampleHDRCS(L"Bloom Extract and DownSample HDR CS");


	ComputePSO ToneMapCS(L"Tone Map  CS");
	ComputePSO ToneMapHDRCS(L"Tone Map HDR CS");
	
	ComputePSO DebugLuminanceHDRCS(L"Debug Luminance HDR CS");
	ComputePSO DebugLuminanceLDRCS(L"Debug Luminance LDR CS");
	ComputePSO GenerateLuminanceHistogramCS(L"Generate Luminance Histogram CS");


	ComputePSO DrawHistogramCS(L"Draw Histogram CS");
	ComputePSO AdaptExposureCS(L"Adapt Exposure CS");

	//Blur
	ComputePSO UpsampleBlurCS(L"UpSample and Blur CS");
	ComputePSO BlurCS(L"Blur CS");

	ComputePSO ExtractLuminanceCS(L"Extract Luminance CS");
	ComputePSO AverageLumaCS(L"Average Luminance CS");
	ComputePSO CopyBackBufferForNotHDRUAVSupportCS(L"Copy Back Post Buffer CS For Not HDR UAV Support");

	void GenerateBloom(ComputeContext& Context);
	void ExtractLuminance(ComputeContext& Context);
	void CopyBackBufferForNotHDRUAVSupport(ComputeContext& Context);
	void PostProcessHDR(ComputeContext& Context);

	void Adaptation(ComputeContext& Context);
}


void ScreenProcessing::Initialize(ID3D12GraphicsCommandList* CommandList)
{
	PostEffectsRS.Reset(4, 2);
	PostEffectsRS.InitStaticSampler(0, SamplerLinearClampDesc);
	PostEffectsRS.InitStaticSampler(1, SamplerLinearBorderDesc);
	PostEffectsRS[0].InitAsConstants(0, 5);
	PostEffectsRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 4);
	PostEffectsRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4);
	PostEffectsRS[3].InitAsConstantBuffer(1);
	PostEffectsRS.Finalize(L"Post Effects");

	//Create Compute PSO Macro
#define CreatePSO( ObjName, ShaderByteCode ) \
    ObjName.SetRootSignature(PostEffectsRS); \
    ObjName.SetComputeShader(ShaderByteCode, sizeof(ShaderByteCode) ); \
    ObjName.Finalize();

	if (g_bTypedUAVLoadSupport_R11G11B10_FLOAT)
	{
		CreatePSO(ToneMapCS, g_pToneMap2CS);
		CreatePSO(ToneMapHDRCS, g_pToneMapHDR2CS);
	}
	else
	{
		CreatePSO(ToneMapCS, g_pToneMapCS);
		CreatePSO(ToneMapHDRCS, g_pToneMapHDRCS);
	}

	CreatePSO(GenerateLuminanceHistogramCS, g_pGenerateLuminanceHistogramCS);
	CreatePSO(UpsampleBlurCS, g_pUpsampleBlurCS);
	CreatePSO(BlurCS, g_pBlurCS);
	CreatePSO(DownsampleBloom2CS, g_pBloomDownSample2CS);
	CreatePSO(DownsampleBloom4CS, g_pBloomDownSample4CS);
	CreatePSO(BloomExtractAndDownsampleHDRCS, g_pBloomExtractAndDownSampleHDRCS);
	CreatePSO(ExtractLuminanceCS, g_pExtractLuminanceCS);
	CreatePSO(CopyBackBufferForNotHDRUAVSupportCS, g_pCopyPostBufferHDRCS);
	CreatePSO(AdaptExposureCS, g_pAdaptExposureCS);

	CreatePSO(DrawHistogramCS, g_pDrawHistogramCS);

#undef CreatePSO

	__declspec(align(16)) float initExposure[] =
	{
		Exposure, 1.0f / Exposure,InitialMinLog, InitialMaxLog, InitialMaxLog - InitialMinLog, 1.0f / (InitialMaxLog - InitialMinLog),0.0f
	};
	g_Exposure.Create(L"Exposure", 7, 4, initExposure);

	BuildSRV(CommandList);
}

void ScreenProcessing::BuildSRV(ID3D12GraphicsCommandList* CommandList)
{
}

void ScreenProcessing::RenderFBM(GraphicsContext& Context, GraphicsPSO* SpecificPSO)
{
	if (SpecificPSO)
	{
		Context.SetPipelineState(*SpecificPSO);
	}
	else
	{
		Context.SetPipelineState(mPSO);
	}
	Context.SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context.Draw(3);
}

void ScreenProcessing::PostProcessHDR(ComputeContext& Context)
{
	ScopedTimer Scope(L"HDR Tone Mapping", Context);

	if (BloomEnable)
	{
		GenerateBloom(Context);
		Context.TransitionResource(g_aBloomUAV1[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}
	else if (EnableAdaptation)
	{
		ExtractLuminance(Context);
	}

	if (g_bTypedUAVLoadSupport_R11G11B10_FLOAT)
	{
		Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}
	else
	{
		Context.TransitionResource(g_PostEffectsBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	Context.TransitionResource(g_LumaBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(g_Exposure, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	Context.SetPipelineState(Display::g_bEnableHDROutput ? ToneMapHDRCS : ToneMapCS);

	// Set constants
	Context.SetConstants(0, 1.0f / g_SceneColorBuffer.GetWidth(), 1.0f / g_SceneColorBuffer.GetHeight(),(float)BloomStrength);
	Context.SetConstant(0, (float)Display::g_HDRPaperWhite / (float)Display::g_MaxDisplayLuminance, 3);
	Context.SetConstant(0, (float)Display::g_MaxDisplayLuminance, 4);

	// Separate out SDR result from its perceived luminance
	if (g_bTypedUAVLoadSupport_R11G11B10_FLOAT)
	{
		Context.SetDynamicDescriptor(1, 0, g_SceneColorBuffer.GetUAV());
	}
	else
	{
		Context.SetDynamicDescriptor(1, 0, g_PostEffectsBuffer.GetUAV());
		Context.SetDynamicDescriptor(2, 2, g_SceneColorBuffer.GetSRV());
	}
	Context.SetDynamicDescriptor(1, 1, g_LumaBuffer.GetUAV());

	// Read in original HDR value and blurred bloom buffer
	Context.SetDynamicDescriptor(2, 0, g_Exposure.GetSRV());
	Context.SetDynamicDescriptor(2, 1, BloomEnable ? g_aBloomUAV1[1].GetSRV() : GetDefaultTexture(eBlackOpaque2D));

	Context.Dispatch2D(g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight());

	//Luminance Adaptation
	Adaptation(Context);
}

void ScreenProcessing::Adaptation(ComputeContext& Context)
{
	ScopedTimer Scope(L"Update Exposure", Context);

	// Generate an HDR Histogram
	Context.TransitionResource(g_Histogram, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
	Context.ClearUAV(g_Histogram);
	Context.TransitionResource(g_LumaBloom, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.SetDynamicDescriptor(1, 0, g_Histogram.GetUAV());
	Context.SetDynamicDescriptor(2, 0, g_LumaBloom.GetSRV());
	Context.SetPipelineState(GenerateLuminanceHistogramCS);
	Context.Dispatch2D(g_LumaBloom.GetWidth(), g_LumaBloom.GetHeight(), 16, 16);
	
	AdaptationConstants AdaptationData = {
		TargetLuminance, AdaptationTranform, MinExposure, MaxExposure,
		g_LumaBloom.GetWidth() * g_LumaBloom.GetHeight()
	};

	Context.TransitionResource(g_Histogram, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(g_Exposure, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.SetDynamicDescriptor(1, 0, g_Exposure.GetUAV());
	Context.SetDynamicDescriptor(2, 0, g_Histogram.GetSRV());
	Context.SetDynamicConstantBufferView(3, sizeof(AdaptationConstants), &AdaptationData);
	Context.SetPipelineState(AdaptExposureCS);
	Context.Dispatch();
	Context.TransitionResource(g_Exposure, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}


void ScreenProcessing::Render()
{
	ComputeContext& Context = ComputeContext::Begin(L"Post Processing");

	Context.SetRootSignature(PostEffectsRS);

	Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

	PostProcessHDR(Context);

	if (!g_bTypedUAVLoadSupport_R11G11B10_FLOAT)
	{
		CopyBackBufferForNotHDRUAVSupport(Context);
	}

	if (DrawHistogram)
	{
		ScopedTimer Scope(L"Draw Debug Histogram", Context);
		Context.SetRootSignature(PostEffectsRS);
		Context.SetPipelineState(DrawHistogramCS);
		Context.InsertUAVBarrier(g_SceneColorBuffer);
		Context.TransitionResource(g_Histogram, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_Exposure, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.SetDynamicDescriptor(1, 0, g_SceneColorBuffer.GetUAV());
		D3D12_CPU_DESCRIPTOR_HANDLE SRVs[2] = { g_Histogram.GetSRV(), g_Exposure.GetSRV() };
		Context.SetDynamicDescriptors(2, 0, 2, SRVs);
		Context.Dispatch(1, 32);
		Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

#ifdef DEBUG
	EngineGUI::AddRenderPassVisualizeTexture("Scene Color", WStringToString(g_SceneColorBuffer.GetName()), g_SceneColorBuffer.GetHeight(), g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetSRV());
	EngineGUI::AddRenderPassVisualizeTexture("Scene Color", WStringToString(g_LumaBuffer.GetName()), g_LumaBuffer.GetHeight(), g_LumaBuffer.GetWidth(), g_LumaBuffer.GetSRV());
	EngineGUI::AddRenderPassVisualizeTexture("Bloom", WStringToString(g_aBloomUAV1[0].GetName()), g_aBloomUAV1[0].GetHeight(), g_aBloomUAV1[0].GetWidth(), g_aBloomUAV1[0].GetSRV());
	EngineGUI::AddRenderPassVisualizeTexture("Bloom", WStringToString(g_aBloomUAV2[0].GetName()), g_aBloomUAV2[0].GetHeight(), g_aBloomUAV2[0].GetWidth(), g_aBloomUAV2[0].GetSRV());
	EngineGUI::AddRenderPassVisualizeTexture("Bloom", WStringToString(g_aBloomUAV3[0].GetName()), g_aBloomUAV3[0].GetHeight(), g_aBloomUAV3[0].GetWidth(), g_aBloomUAV3[0].GetSRV());
	EngineGUI::AddRenderPassVisualizeTexture("Bloom", WStringToString(g_aBloomUAV4[0].GetName()), g_aBloomUAV4[0].GetHeight(), g_aBloomUAV4[0].GetWidth(), g_aBloomUAV4[0].GetSRV());
#endif

	Context.Finish();
}

void ScreenProcessing::DrawBeforeToneMapping()
{
	GraphicsContext& Context = GraphicsContext::Begin(L"Before Tone Mapping");
	Context.PIXBeginEvent(L"Before Tone Mapping");
	Context.PIXEndEvent();
	Context.Finish();
}

void ScreenProcessing::DrawAfterToneMapping()
{
	GraphicsContext& Context = GraphicsContext::Begin(L"After Tone Mapping");
	Context.PIXBeginEvent(L"After Tone Mapping");
	//FBM
	//Context.SetDynamicConstantBufferView((int)RootSignatureType::eMainConstantBuffer, sizeof(mMainConstants), &mMainConstants);
	//RenderFBM(Context);


	Context.PIXEndEvent();
	Context.Finish();
}

void ScreenProcessing::DrawUI()
{
	ImGuiIO& io = ImGui::GetIO();
	if (ImGui::CollapsingHeader("Post Processing", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::TreeNodeEx("Bloom", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Enable Bloom", &BloomEnable);
			if (BloomEnable)
			{
				ImGui::Checkbox("High Quality Bloom", &HighQualityBloom);
				ImGui::SliderVerticalFloat("Bloom Threshold:", &BloomThreshold.GetValue(), BloomThreshold.GetMinValue(), BloomThreshold.GetMaxValue());
				ImGui::SliderVerticalFloat("Bloom Strength:", &BloomStrength.GetValue(), BloomStrength.GetMinValue(), BloomStrength.GetMaxValue());
				ImGui::SliderVerticalFloat("Bloom UpSample Factor:", &BloomUpSampleFactor.GetValue(), BloomUpSampleFactor.GetMinValue(), BloomUpSampleFactor.GetMaxValue());
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Luminance", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderVerticalFloat("Target Luminance:", &TargetLuminance.GetValue(), TargetLuminance.GetMinValue(), TargetLuminance.GetMaxValue());
			ImGui::Checkbox("Draw Luminance Histogram", &DrawHistogram);
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("FXAA"))
		{
			ImGui::TreePop();
		}

		

	}
}

void ScreenProcessing::Update(float dt)
{
}

void ScreenProcessing::BuildPSO(const PSOCommonProperty CommonProperty)
{
	D3D12_RASTERIZER_DESC Rasterizer = RasterizerDefault;
	if (CommonProperty.IsWireframe)
	{
		Rasterizer.CullMode = D3D12_CULL_MODE_NONE;
		Rasterizer.FillMode = D3D12_FILL_MODE_WIREFRAME;
	}

	mPSO.SetRootSignature(*CommonProperty.pRootSignature);
	mPSO.SetRasterizerState(Rasterizer);
	mPSO.SetBlendState(BlendDisable);
	mPSO.SetDepthStencilState(DepthStateDisabled);
	mPSO.SetSampleMask(0xFFFFFFFF);
	mPSO.SetInputLayout((UINT)InputLayout::Pos.size(), InputLayout::Pos.data());
	mPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	mPSO.SetVertexShader(g_pScreenQuadVS, sizeof(g_pScreenQuadVS));
	mPSO.SetPixelShader(g_pFbmPostPS, sizeof(g_pFbmPostPS));
	mPSO.SetRenderTargetFormat(DefaultHDRColorFormat, g_SceneDepthBuffer.GetFormat(), CommonProperty.MSAACount, CommonProperty.MSAAQuality);
	mPSO.Finalize();
}

void ScreenProcessing::UpsampleBlurBuffer(ComputeContext& Context, ColorBuffer buffer[2], const ColorBuffer& LowerResBuffer, float UpSampleBlendFactor)
{
	// Set the shader constants
	uint32_t bufferWidth = buffer[0].GetWidth();
	uint32_t bufferHeight = buffer[0].GetHeight();
	Context.SetConstants(0, 1.0f / bufferWidth, 1.0f / bufferHeight, UpSampleBlendFactor);

	// Set the input textures and output UAV
	Context.TransitionResource(buffer[1], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.SetDynamicDescriptor(1, 0, buffer[1].GetUAV());
	D3D12_CPU_DESCRIPTOR_HANDLE SRVs[2] = { buffer[0].GetSRV(), LowerResBuffer.GetSRV() };
	Context.SetDynamicDescriptors(2, 0, 2, SRVs);

	// Set the shader: Up-Sample and blur
	Context.SetPipelineState(UpsampleBlurCS);

	// Dispatch the compute shader with default 8x8 thread groups
	Context.Dispatch2D(bufferWidth, bufferHeight);

	Context.TransitionResource(buffer[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void ScreenProcessing::BlurBuffer(ComputeContext& Context, ColorBuffer& SourceBuffer, ColorBuffer& TargetBuffer)
{
	// Set the shader constants
	uint32_t bufferWidth = SourceBuffer.GetWidth();
	uint32_t bufferHeight = SourceBuffer.GetHeight();
	Context.SetConstants(0, 1.0f / bufferWidth, 1.0f / bufferHeight);

	// Set the input textures and output UAV
	Context.TransitionResource(TargetBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.SetDynamicDescriptor(1, 0, TargetBuffer.GetUAV());
	Context.SetDynamicDescriptor(2, 0, SourceBuffer.GetSRV());

	// Set the shader
	Context.SetPipelineState(BlurCS);

	// Dispatch the compute shader with default 8x8 thread groups
	Context.Dispatch2D(bufferWidth, bufferHeight);

	Context.TransitionResource(TargetBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}


void ScreenProcessing::ShutDown()
{
	g_Exposure.Destroy();
}


void ScreenProcessing::GenerateBloom(ComputeContext& Context)
{
	ScopedTimer Scope(L"Generate Bloom", Context);

	// If only downsizing by 1/2 or less, a faster shader can be used which only does one bilinear sample.
	uint32_t BloomWidth = g_LumaBloom.GetWidth();
	uint32_t BloomHeight = g_LumaBloom.GetHeight();

	Context.SetConstants(0, 1.0f / BloomWidth, 1.0f / BloomHeight, (float)BloomThreshold);
	Context.TransitionResource(g_LumaBloom, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(g_aBloomUAV1[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(g_Exposure, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	Context.SetDynamicDescriptor(1, 0, g_aBloomUAV1[0].GetUAV());
	Context.SetDynamicDescriptor(1, 1, g_LumaBloom.GetUAV());
	Context.SetDynamicDescriptor(2, 0, g_SceneColorBuffer.GetSRV());
	Context.SetDynamicDescriptor(2, 1, g_Exposure.GetSRV());

	//Bloom Extract
	Context.SetPipelineState(BloomExtractAndDownsampleHDRCS);
	Context.Dispatch2D(BloomWidth, BloomHeight);

	Context.TransitionResource(g_aBloomUAV1[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.SetDynamicDescriptor(2, 0, g_aBloomUAV1[0].GetSRV());


	//High Quality:Sums 5 Octaves with a 2x frequency scale
	//Low Quality :Sums 3 Octaves with a 4x frequency scale
	if (HighQualityBloom)
	{
		Context.TransitionResource(g_aBloomUAV2[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		Context.TransitionResource(g_aBloomUAV3[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		Context.TransitionResource(g_aBloomUAV4[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		Context.TransitionResource(g_aBloomUAV5[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		// Set the UAVs
		D3D12_CPU_DESCRIPTOR_HANDLE UAVs[4] = {g_aBloomUAV2[0].GetUAV(), g_aBloomUAV3[0].GetUAV(), g_aBloomUAV4[0].GetUAV(), g_aBloomUAV5[0].GetUAV() };
		Context.SetDynamicDescriptors(1, 0, 4, UAVs);

		// Each dispatch group is 8x8 threads,Each dispatch group is 8x8 threads, Each thread reads in 2x2 source Texels use bilinear filter.
		Context.SetPipelineState(DownsampleBloom4CS);
		Context.Dispatch2D(BloomWidth / 2, BloomHeight / 2);

		Context.TransitionResource(g_aBloomUAV2[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_aBloomUAV3[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_aBloomUAV4[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_aBloomUAV5[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		// Blur then Up-sampling and blur four times
		BlurBuffer(Context, g_aBloomUAV5[0], g_aBloomUAV5[1]);
		UpsampleBlurBuffer(Context, g_aBloomUAV4, g_aBloomUAV5[1], BloomUpSampleFactor);
		UpsampleBlurBuffer(Context, g_aBloomUAV3, g_aBloomUAV4[1], BloomUpSampleFactor);
		UpsampleBlurBuffer(Context, g_aBloomUAV2, g_aBloomUAV3[1], BloomUpSampleFactor);
		UpsampleBlurBuffer(Context, g_aBloomUAV1, g_aBloomUAV2[1], BloomUpSampleFactor);
	}
	else
	{
		Context.TransitionResource(g_aBloomUAV3[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		Context.TransitionResource(g_aBloomUAV5[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		// Set the UAVs
		D3D12_CPU_DESCRIPTOR_HANDLE UAVs[2] = { g_aBloomUAV3[0].GetUAV(), g_aBloomUAV5[0].GetUAV() };
		Context.SetDynamicDescriptors(1, 0, 2, UAVs);

		Context.SetPipelineState(DownsampleBloom2CS);
		Context.Dispatch2D(BloomWidth / 2, BloomHeight / 2);

		Context.TransitionResource(g_aBloomUAV3[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_aBloomUAV5[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		// Blur then Up-sampling and blur two times
		BlurBuffer(Context, g_aBloomUAV5[0], g_aBloomUAV5[1]);
		UpsampleBlurBuffer(Context, g_aBloomUAV3, g_aBloomUAV5[1], BloomUpSampleFactor);
		UpsampleBlurBuffer(Context, g_aBloomUAV1, g_aBloomUAV3[1], BloomUpSampleFactor);
	}

}

void ScreenProcessing::ExtractLuminance(ComputeContext& Context)
{
	ScopedTimer Scope(L"Extract Luminance", Context);
	Context.TransitionResource(g_LumaBloom, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(g_Exposure, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.SetConstants(0, 1.0f / g_LumaBloom.GetWidth(), 1.0f / g_LumaBloom.GetHeight());
	Context.SetDynamicDescriptor(1, 0, g_LumaBloom.GetUAV());
	Context.SetDynamicDescriptor(2, 0, g_SceneColorBuffer.GetSRV());
	Context.SetDynamicDescriptor(2, 1, g_Exposure.GetSRV());
	Context.SetPipelineState(ExtractLuminanceCS);
	Context.Dispatch2D(g_LumaBloom.GetWidth(), g_LumaBloom.GetHeight());
}


void ScreenProcessing::CopyBackBufferForNotHDRUAVSupport(ComputeContext& Context)
{
	ScopedTimer Scope(L"Copy Post back to Scene For Not HDR UAV Support", Context);
	Context.SetRootSignature(PostEffectsRS);
	Context.SetPipelineState(CopyBackBufferForNotHDRUAVSupportCS);
	Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(g_PostEffectsBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.SetDynamicDescriptor(1, 0, g_SceneColorBuffer.GetUAV());
	Context.SetDynamicDescriptor(2, 0, g_PostEffectsBuffer.GetSRV());
	Context.Dispatch2D(g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight());
}