#include "Engine/EngineProfiling.h"
#include "PostProcessing.h"
#include "Graphics/GraphicsCommon.h"
#include "Graphics/SamplerManager.h"
#include "Graphics/CommandSignature.h"
#include "EngineGUI.h"

//shaders
#include "CompiledShaders/ScreenQuadVS.h"
#include "CompiledShaders/FbmPostPS.h"
#include "CompiledShaders/ApplyBloomCS.h"
#include "CompiledShaders/ApplyBloom2CS.h"
#include "CompiledShaders/BloomDownSample2CS.h"
#include "CompiledShaders/BloomDownSample4CS.h"
#include "CompiledShaders/BloomExtractAndDownSampleHDRCS.h"
#include "CompiledShaders/BloomExtractAndDownSampleLDRCS.h"

namespace PostProcessing
{

	bool  BloomEnable = true;
	bool  HighQualityBloom = true;					// High quality blurs 5 octaves of bloom; low quality only blurs 3.
	NumVar BloomThreshold(4.0f, 0.0f, 8.0f);		// The threshold luminance above which a pixel will start to bloom
	NumVar BloomStrength(0.1f, 0.0f, 2.0f);			// A modulator controlling how much bloom is added back into the image
	NumVar BloomUpSampleFactor(0.65f, 0.0f, 1.0f);	// Controls the "focus" of the blur.High values spread out more causing a haze.

	//Exposure Log() Range
	const float kInitialMinLog = -12.0f;
	const float kInitialMaxLog = 4.0f;

	bool EnableHDR = true;
	bool EnableAdaptation = true;
	bool DrawHistogram = true;
	NumVar TargetLuminance(0.08f, 0.01f, 0.99f);
	NumVar AdaptationRate(0.05f, 0.01f, 1.0f);
	NumVar Exposure(2.0f, -8.0f, 8.0f);
	
	//Exposure Data
	StructuredBuffer g_Exposure;

	RootSignature PostEffectsRS;
	GraphicsPSO mPSO(L"FBM Post PS");

	//Bloom
	ComputePSO ApplyBloomCS(L"Apply Bloom CS");
	ComputePSO DownsampleBloom2CS(L"DownSample Bloom 2 CS");
	ComputePSO DownsampleBloom4CS(L"DownSample Bloom 4 CS");
	ComputePSO BloomExtractAndDownsampleHDRCS(L"Bloom Extract and DownSample HDR CS");
	ComputePSO BloomExtractAndDownsampleLDRCS(L"Bloom Extract and DownSample LDR CS");


	ComputePSO ToneMapCS(L"Tone Map  CS");
	ComputePSO ToneMapHDRCS(L"Tone Map HDR CS");
	
	ComputePSO DebugLuminanceHDRCS(L"Debug Luminance HDR CS");
	ComputePSO DebugLuminanceLDRCS(L"Debug Luminance LDR CS");
	ComputePSO GenerateHistogramCS(L"Generate Histogram CS");
	ComputePSO DrawHistogramCS(L"Draw Histogram CS");
	ComputePSO AdaptExposureCS(L"Adapt Exposure CS");

	//Blur
	ComputePSO UpsampleAndBlurCS(L"UpSample and Blur CS");
	ComputePSO BlurCS(L"Blur CS");

	ComputePSO ExtractLumaCS(L"Extract Luminance CS");
	ComputePSO AverageLumaCS(L"Average Luminance CS");
	ComputePSO CopyBackPostBufferCS(L"Copy Back Post Buffer CS");


	// Bloom effect
	void GenerateBloom(ComputeContext& Context);


	void PostProcessHDR(ComputeContext&);
	void PostProcessLDR(ComputeContext&);
}


void PostProcessing::Initialize(ID3D12GraphicsCommandList* CommandList)
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
		CreatePSO(ApplyBloomCS, g_pApplyBloom2CS);
		//CreatePSO(ToneMapCS, g_pToneMap2CS);
		//CreatePSO(ToneMapHDRCS, g_pToneMapHDR2CS);
		//CreatePSO(DebugLuminanceHdrCS, g_pDebugLuminanceHdr2CS);
		//CreatePSO(DebugLuminanceLdrCS, g_pDebugLuminanceLdr2CS);
	}
	else
	{
		CreatePSO(ApplyBloomCS, g_pApplyBloomCS);
		//CreatePSO(ToneMapCS, g_pToneMapCS);
		//CreatePSO(ToneMapHDRCS, g_pToneMapHDRCS);
		//CreatePSO(DebugLuminanceHdrCS, g_pDebugLuminanceHdrCS);
		//CreatePSO(DebugLuminanceLdrCS, g_pDebugLuminanceLdrCS);
	}

	/*CreatePSO(UpsampleAndBlurCS, g_pUpsampleAndBlurCS);
	CreatePSO(BlurCS, g_pBlurCS);
	CreatePSO(GenerateHistogramCS, g_pGenerateHistogramCS);
	CreatePSO(DrawHistogramCS, g_pDebugDrawHistogramCS);
	CreatePSO(AdaptExposureCS, g_pAdaptExposureCS);*/
	CreatePSO(DownsampleBloom2CS, g_pBloomDownSample2CS);
	CreatePSO(DownsampleBloom4CS, g_pBloomDownSample4CS);
	CreatePSO(BloomExtractAndDownsampleHDRCS, g_pBloomExtractAndDownSampleHDRCS);
	CreatePSO(BloomExtractAndDownsampleLDRCS, g_pBloomExtractAndDownSampleLDRCS);

	//CreatePSO(ExtractLumaCS, g_pExtractLumaCS);
	//CreatePSO(AverageLumaCS, g_pAverageLumaCS);
	//CreatePSO(CopyBackPostBufferCS, g_pCopyBackPostBufferCS);

#undef CreatePSO

	__declspec(align(16)) float initExposure[] =
	{
		Exposure, 1.0f / Exposure,kInitialMinLog, kInitialMaxLog, kInitialMaxLog - kInitialMinLog, 1.0f / (kInitialMaxLog - kInitialMinLog)
	};
	g_Exposure.Create(L"Exposure", 6, 4, initExposure);

	BuildSRV(CommandList);
}

void PostProcessing::BuildSRV(ID3D12GraphicsCommandList* CommandList)
{
}

void PostProcessing::RenderFBM(GraphicsContext& Context, GraphicsPSO* SpecificPSO)
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

void PostProcessing::PostProcessHDR(ComputeContext& Context)
{

}

void PostProcessing::PostProcessLDR(ComputeContext& Context)
{

}


void PostProcessing::Render(GraphicsContext& graphicsContext)
{
	ComputeContext& Context = ComputeContext::Begin(L"Post Processing");

	Context.SetRootSignature(PostEffectsRS);
	Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	if (EnableHDR)
		PostProcessHDR(Context);
	else
		PostProcessLDR(Context);
}

void PostProcessing::DrawBeforeToneMapping()
{
	GraphicsContext& Context = GraphicsContext::Begin(L"Before Tone Mapping");
	Context.PIXBeginEvent(L"Before Tone Mapping");
	Context.PIXEndEvent();
	Context.Finish();
}

void PostProcessing::DrawAfterToneMapping()
{
	GraphicsContext& Context = GraphicsContext::Begin(L"After Tone Mapping");
	Context.PIXBeginEvent(L"After Tone Mapping");
	//FBM
	//Context.SetDynamicConstantBufferView((int)RootSignatureType::eMainConstantBuffer, sizeof(mMainConstants), &mMainConstants);
	//RenderFBM(Context);


	Context.PIXEndEvent();
	Context.Finish();
}

void PostProcessing::DrawUI()
{
	ImGuiIO& io = ImGui::GetIO();
	if (ImGui::CollapsingHeader("Post Processing"))
	{
		if (ImGui::TreeNode("Bloom"))
		{
			ImGui::Checkbox("Enable Bloom", &BloomEnable);
			if (BloomEnable)
			{
				ImGui::Checkbox("High Quality Bloom", &HighQualityBloom);
				ImGui::Text("Bloom Threshold:");
				ImGui::SliderFloat(" ", &BloomThreshold.GetValue(), BloomThreshold.GetMinValue(), BloomThreshold.GetMaxValue());
				ImGui::Text("Bloom Strength:");
				ImGui::SliderFloat("  ", &BloomStrength.GetValue(), BloomStrength.GetMinValue(), BloomStrength.GetMaxValue());
				ImGui::Text("Bloom UpSample Factor:");
				ImGui::SliderFloat("   ", &BloomUpSampleFactor.GetValue(), BloomUpSampleFactor.GetMinValue(), BloomUpSampleFactor.GetMaxValue());
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("FXAA"))
		{
			ImGui::TreePop();
		}

	}
}

void PostProcessing::Update(float dt)
{
}

void PostProcessing::BuildPSO(const PSOCommonProperty CommonProperty)
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

void PostProcessing::UpsampleBlurBuffer(ComputeContext& Context, ColorBuffer buffer[2], const ColorBuffer& LowerResBuffer, float UpSampleBlendFactor)
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

	// Set the shader: Upsample and blur
	Context.SetPipelineState(UpsampleAndBlurCS);

	// Dispatch the compute shader with default 8x8 thread groups
	Context.Dispatch2D(bufferWidth, bufferHeight);

	Context.TransitionResource(buffer[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void PostProcessing::BlurBuffer(ComputeContext& Context, ColorBuffer SourceBuffer, ColorBuffer TargetBuffer)
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

void PostProcessing::ShutDown()
{
	g_Exposure.Destroy();
}


void PostProcessing::GenerateBloom(ComputeContext& Context)
{
	ScopedTimer _prof(L"Generate Bloom", Context);

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
	Context.SetPipelineState(EnableHDR ? BloomExtractAndDownsampleHDRCS : BloomExtractAndDownsampleLDRCS);
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

	}
	else
	{
		Context.TransitionResource(g_aBloomUAV3[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		Context.TransitionResource(g_aBloomUAV5[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		// Set the UAVs
		D3D12_CPU_DESCRIPTOR_HANDLE UAVs[2] = { g_aBloomUAV3[0].GetUAV(), g_aBloomUAV5[0].GetUAV() };
		Context.SetDynamicDescriptors(1, 0, 2, UAVs);
	}

}
