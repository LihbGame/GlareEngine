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
#include "CompiledShaders/BloomDownSampleCS.h"
#include "CompiledShaders/BloomDownSampleAllCS.h"
#include "CompiledShaders/BloomExtractAndDownSampleHDRCS.h"
#include "CompiledShaders/BloomExtractAndDownSampleLDRCS.h"

namespace PostProcessing
{

	bool  BloomEnable = true;
	bool  HighQualityBloom = true;					// High quality blurs 5 octaves of bloom; low quality only blurs 3.
	NumVar BloomThreshold(4.0f, 0.0f, 8.0f);		// The threshold luminance above which a pixel will start to bloom
	NumVar BloomStrength(0.1f, 0.0f, 2.0f);			// A modulator controlling how much bloom is added back into the image
	NumVar BloomUpsampleFactor(0.65f, 0.0f, 1.0f);	// Controls the "focus" of the blur.  High values spread out more causing a haze.


	bool EnableHDR = true;
	bool EnableAdaptation = true;
	bool DrawHistogram = true;
	NumVar TargetLuminance(0.08f, 0.01f, 0.99f);
	NumVar AdaptationRate(0.05f, 0.01f, 1.0f);
	NumVar Exposure(2.0f, -8.0f, 8.0f);
	

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

	ComputePSO UpsampleAndBlurCS(L"UpSample and Blur CS");
	ComputePSO BlurCS(L"Blur CS");

	ComputePSO ExtractLumaCS(L"Extract Luminance CS");
	ComputePSO AverageLumaCS(L"Average Luminance CS");
	ComputePSO CopyBackPostBufferCS(L"Copy Back Post Buffer CS");




	// Bloom effect
	void GenerateBloom(ComputeContext& Context);

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
	CreatePSO(DownsampleBloom2CS, g_pBloomDownSampleCS);
	CreatePSO(DownsampleBloom4CS, g_pBloomDownSampleAllCS);
	CreatePSO(BloomExtractAndDownsampleHDRCS, g_pBloomExtractAndDownSampleHDRCS);
	CreatePSO(BloomExtractAndDownsampleLDRCS, g_pBloomExtractAndDownSampleLDRCS);

	//CreatePSO(ExtractLumaCS, g_pExtractLumaCS);
	//CreatePSO(AverageLumaCS, g_pAverageLumaCS);
	//CreatePSO(CopyBackPostBufferCS, g_pCopyBackPostBufferCS);

#undef CreatePSO



	BuildSRV(CommandList);
}

void PostProcessing::BuildSRV(ID3D12GraphicsCommandList* CommandList)
{
}

void PostProcessing::Render(GraphicsContext& Context, GraphicsPSO* SpecificPSO)
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

void PostProcessing::DrawBeforeToneMapping()
{
	GraphicsContext& Context = GraphicsContext::Begin(L"PostProcessing");
	Context.PIXBeginEvent(L"PostProcessing");
	Render(Context);
	Context.PIXEndEvent();
	Context.Finish();
}

void PostProcessing::DrawAfterToneMapping()
{
	GraphicsContext& Context = GraphicsContext::Begin(L"PostProcessing");
	Context.PIXBeginEvent(L"PostProcessing");
	//Context.SetDynamicConstantBufferView((int)RootSignatureType::eMainConstantBuffer, sizeof(mMainConstants), &mMainConstants);
	Render(Context);
	Context.PIXEndEvent();
	Context.Finish();
}

void PostProcessing::DrawUI()
{
	ImGuiIO& io = ImGui::GetIO();
	if (ImGui::CollapsingHeader("Post Processing", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::TreeNode("Bloom"))
		{
			ImGui::Checkbox("Enable Bloom", &BloomEnable);
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


void PostProcessing::GenerateBloom(ComputeContext& Context)
{
	ScopedTimer _prof(L"Generate Bloom", Context);

	// If only downsizing by 1/2 or less, a faster shader can be used which only does one bilinear sample.



}
