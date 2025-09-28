#include "Engine/EngineProfiling.h"
#include "Graphics/GraphicsCommon.h"
#include "Graphics/SamplerManager.h"
#include "Graphics/CommandSignature.h"
#include "Graphics/Display.h"
#include "PostProcessing.h"
#include "EngineGUI.h"
#include "SSAO.h"
#include "FXAA.h"
#include "MotionBlur.h"
#include "TemporalAA.h"
#include "Engine/Scene.h"
#include "Graphics/FSR.h"

//shaders
#include "CompiledShaders/ScreenQuadVS.h"
#include "CompiledShaders/FbmPostPS.h"
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
#include "CompiledShaders/LinearizeDepthCS.h"

#include "CompiledShaders/BilateralBlurFloat1CS.h"
#include "CompiledShaders/BilateralBlurFloat3CS.h"
#include "CompiledShaders/BilateralBlurFloat4CS.h"
#include "CompiledShaders/BilateralBlurUnorm1CS.h"
#include "CompiledShaders/BilateralBlurUnorm4CS.h"
#include "CompiledShaders/BilateralBlurWideFloat1CS.h"
#include "CompiledShaders/BilateralBlurWideFloat3CS.h"
#include "CompiledShaders/BilateralBlurWideFloat4CS.h"
#include "CompiledShaders/BilateralBlurWideUnorm1CS.h"
#include "CompiledShaders/BilateralBlurWideUnorm4CS.h"

#include "CompiledShaders/GaussianBlurFloat1CS.h"
#include "CompiledShaders/GaussianBlurFloat3CS.h"
#include "CompiledShaders/GaussianBlurFloat4CS.h"
#include "CompiledShaders/GaussianBlurUnorm1CS.h"
#include "CompiledShaders/GaussianBlurUnorm4CS.h"
#include "CompiledShaders/GaussianBlurWideFloat1CS.h"
#include "CompiledShaders/GaussianBlurWideFloat3CS.h"
#include "CompiledShaders/GaussianBlurWideFloat4CS.h"
#include "CompiledShaders/GaussianBlurWideUnorm1CS.h"
#include "CompiledShaders/GaussianBlurWideUnorm4CS.h"


namespace ScreenProcessing
{
	enum BlurShaderType	:int
	{
		BilateralBlurFloat1,
		BilateralBlurFloat3,
		BilateralBlurFloat4,
		BilateralBlurUnorm1,
		BilateralBlurUnorm4,
		BilateralBlurWideFloat1,
		BilateralBlurWideFloat3,
		BilateralBlurWideFloat4,
		BilateralBlurWideUnorm1,
		BilateralBlurWideUnorm4,
		GaussianBlurFloat1,
		GaussianBlurFloat3,
		GaussianBlurFloat4,
		GaussianBlurUnorm1,
		GaussianBlurUnorm4,
		GaussianBlurWideFloat1,
		GaussianBlurWideFloat3,
		GaussianBlurWideFloat4,
		GaussianBlurWideUnorm1,
		GaussianBlurWideUnorm4,
		Count
	};

	__declspec(align(16)) struct AdaptationConstants
	{
		float TargetLuminance;
		float AdaptationRate;
		float MinExposure;
		float MaxExposure;
		uint32_t PixelCount;
	};

	__declspec(align(16)) struct BlurConstants
	{
		XMFLOAT2	InverseDimensions;
		XMINT2		Dimensions;
		int			IsHorizontalBlur;
		float		CameraFar;
	};

	ColorBuffer* LastPostprocessRT		= nullptr;
	ColorBuffer* CurrentPostprocessRT	= nullptr;

	bool  EnableEyeAdaptation		= true;
	bool  BloomEnable				= true;
	bool  HighQualityBloom			= true;			// High quality blurs 5 octaves of bloom; low quality only blurs 3.
	NumVar BloomThreshold(1.5f, 0.0f, 8.0f);		// The threshold luminance above which a pixel will start to bloom
	NumVar BloomStrength(0.5f, 0.0f, 2.0f);			// A modulator controlling how much bloom is added back into the image
	NumVar BloomUpSampleFactor(0.65f, 0.0f, 1.0f);	// Controls the "focus" of the blur.High values spread out more causing a haze.

	//Exposure Log() Range
	const float InitialMinLog		= -12.0f;
	const float InitialMaxLog		= 4.0f;

	bool EnableAdaptation			= true;
	bool DrawHistogram				= false;

	NumVar TargetLuminance(0.05f, 0.01f, 0.99f);
	NumVar AdaptationTranform(0.02f, 0.01f, 1.0f);
	NumVar Exposure(2.0f, -8.0f, 8.0f);
	
	float MinExposure = 1.0f / 64.0f;
	float MaxExposure = 64.0f;

	string mAntiAliasingName;
	int mAntiAliasingIndex = Render::AntiAliasingType::FSR;
	int mPreAntiAliasingIndex= Render::AntiAliasingType::FSR;
	//Exposure Data
	StructuredBuffer g_Exposure;

	RootSignature PostEffectsRS;

	RenderMaterial* mFBMMaterial = nullptr;

	MainConstants* gMainConstants = nullptr;

	ColorBuffer* CurrentLinearDepth = nullptr;

	RenderMaterial* ShaderMaterials[BlurShaderType::Count] = {nullptr};

	//Bloom
	RenderMaterial* DownsampleBloom2CS = nullptr;
	RenderMaterial* DownsampleBloom4CS = nullptr;
	RenderMaterial* BloomExtractAndDownsampleHDRCS = nullptr;

	RenderMaterial* ToneMapCS = nullptr;
	RenderMaterial* ToneMapHDRCS = nullptr;
	
	RenderMaterial* DebugLuminanceHDRCS = nullptr;
	RenderMaterial* DebugLuminanceLDRCS = nullptr;
	RenderMaterial* GenerateLuminanceHistogramCS = nullptr;

	RenderMaterial* DrawHistogramCS = nullptr;
	RenderMaterial* AdaptExposureCS = nullptr;

	//Blur
	RenderMaterial* UpsampleBlurCS = nullptr;
	RenderMaterial* BlurCS = nullptr;

	RenderMaterial* ExtractLuminanceCS = nullptr;
	RenderMaterial* AverageLumaCS = nullptr;
	RenderMaterial* CopyBackBufferForNotHDRUAVSupportCS = nullptr;

	//Linear Depth PSO
	RenderMaterial* LinearizeDepthCS = nullptr;

	//Blur PSO
	RenderMaterial* BilateralBlurFloat1CS = nullptr;
	RenderMaterial* BilateralBlurFloat3CS = nullptr;
	RenderMaterial* BilateralBlurFloat4CS = nullptr;
	RenderMaterial* BilateralBlurUnorm1CS = nullptr;
	RenderMaterial* BilateralBlurUnorm4CS = nullptr;
	RenderMaterial* BilateralBlurWideFloat1CS = nullptr;
	RenderMaterial* BilateralBlurWideFloat3CS = nullptr;
	RenderMaterial* BilateralBlurWideFloat4CS = nullptr;
	RenderMaterial* BilateralBlurWideUnorm1CS = nullptr;
	RenderMaterial* BilateralBlurWideUnorm4CS = nullptr;
	RenderMaterial* GaussianBlurFloat1CS = nullptr;
	RenderMaterial* GaussianBlurFloat3CS = nullptr;
	RenderMaterial* GaussianBlurFloat4CS = nullptr;
	RenderMaterial* GaussianBlurUnorm1CS = nullptr;
	RenderMaterial* GaussianBlurUnorm4CS = nullptr;
	RenderMaterial* GaussianBlurWideFloat1CS = nullptr;
	RenderMaterial* GaussianBlurWideFloat3CS = nullptr;
	RenderMaterial* GaussianBlurWideFloat4CS = nullptr;
	RenderMaterial* GaussianBlurWideUnorm1CS = nullptr;
	RenderMaterial* GaussianBlurWideUnorm4CS = nullptr;

	void GenerateBloom(ComputeContext& Context);
	void ExtractLuminance(ComputeContext& Context);
	void CopyBackBufferForNotHDRUAVSupport(ComputeContext& Context);
	void ToneMappingHDR(ComputeContext& Context);
	void Adaptation(ComputeContext& Context);
	void InitMaterial();
}


void ScreenProcessing::Initialize(ID3D12GraphicsCommandList* CommandList)
{
	PostEffectsRS.Reset(4, 2);
	PostEffectsRS.InitStaticSampler(0, SamplerLinearClampDesc);
	PostEffectsRS.InitStaticSampler(1, SamplerLinearBorderDesc);
	PostEffectsRS[0].InitAsConstants(0, 5);
	PostEffectsRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 10);
	PostEffectsRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
	PostEffectsRS[3].InitAsConstantBuffer(1);
	PostEffectsRS.Finalize(L"Post Effects");

	InitMaterial();

	mAntiAliasingName += string("FSR") + '\0';
	mAntiAliasingName += string("MSAA") + '\0';
	mAntiAliasingName += string("FXAA") + '\0';
	mAntiAliasingName += string("TAA") + '\0';
	mAntiAliasingName += string("TFAA") + '\0';
	mAntiAliasingName += string("NoAA") + '\0';

	__declspec(align(16)) float initExposure[] =
	{
		Exposure, 1.0f / Exposure,InitialMinLog, InitialMaxLog, InitialMaxLog - InitialMinLog, 1.0f / (InitialMaxLog - InitialMinLog),0.0f
	};
	g_Exposure.Create(L"Exposure", 7, 4, initExposure);

	BuildSRV(CommandList);

	Render::SetAntiAliasingType(Render::AntiAliasingType(mAntiAliasingIndex));

	//FXAA Initialize
	FXAA::Initialize();

	//Motion Blur Initialize
	MotionBlur::Initialize();

	//TemporalAA Initialize
	TemporalAA::Initialize();
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
		Context.SetPipelineState(mFBMMaterial->GetRuntimePSO());
	}
	Context.SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context.Draw(3);
}

void ScreenProcessing::ToneMappingHDR(ComputeContext& Context)
{
	ScopedTimer Scope(L"HDR Tone Mapping", Context);

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

	Context.SetPipelineState(Display::g_bEnableHDROutput ? ToneMapHDRCS->GetRuntimePSO() : ToneMapCS->GetRuntimePSO());

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
	Context.SetPipelineState(GenerateLuminanceHistogramCS->GetRuntimePSO());
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
	Context.SetPipelineState(AdaptExposureCS->GetRuntimePSO());
	Context.Dispatch();
	Context.TransitionResource(g_Exposure, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void ScreenProcessing::InitMaterial()
{
	mFBMMaterial = RenderMaterialManager::GetInstance().GetMaterial("FBM Material");

	if (!mFBMMaterial->IsInitialized)
	{
		mFBMMaterial->BindPSOCreateFunc([&](const PSOCommonProperty CommonProperty) {
			D3D12_RASTERIZER_DESC Rasterizer = RasterizerDefault;
			if (CommonProperty.IsWireframe)
			{
				Rasterizer.CullMode = D3D12_CULL_MODE_NONE;
				Rasterizer.FillMode = D3D12_FILL_MODE_WIREFRAME;
			}

			GraphicsPSO& pso = mFBMMaterial->GetGraphicsPSO();

			pso.SetRootSignature(*CommonProperty.pRootSignature);
			pso.SetRasterizerState(Rasterizer);
			pso.SetBlendState(BlendDisable);
			pso.SetDepthStencilState(DepthStateDisabled);
			pso.SetSampleMask(0xFFFFFFFF);
			pso.SetInputLayout((UINT)InputLayout::Pos.size(), InputLayout::Pos.data());
			pso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			pso.SetVertexShader(g_pScreenQuadVS, sizeof(g_pScreenQuadVS));
			pso.SetPixelShader(g_pFbmPostPS, sizeof(g_pFbmPostPS));
			pso.SetRenderTargetFormat(DefaultHDRColorFormat, g_SceneDepthBuffer.GetFormat(), CommonProperty.MSAACount, CommonProperty.MSAAQuality);
			pso.Finalize();
			});

		mFBMMaterial->BindPSORuntimeModifyFunc([&]() {
			RuntimePSOManager::Get().RegisterPSO(&mFBMMaterial->GetGraphicsPSO(), GET_SHADER_PATH("PostProcessing/FbmPostPS.hlsl"), D3D12_SHVER_PIXEL_SHADER); });

		mFBMMaterial->IsInitialized = true;
	}

#define InitPostProcessMaterial(shaderMaterial,materialName,shaderPath,CodeBinary)\
	shaderMaterial=RenderMaterialManager::GetInstance().GetMaterial(materialName,MaterialPipelineType::Compute);\
	if (!shaderMaterial->IsInitialized)\
{\
	shaderMaterial->BindPSOCreateFunc([&](const PSOCommonProperty CommonProperty) {\
		shaderMaterial->GetComputePSO().SetRootSignature(PostEffectsRS);\
		shaderMaterial->GetComputePSO().SetComputeShader(CodeBinary, sizeof(CodeBinary));\
		shaderMaterial->GetComputePSO().Finalize();\
		});\
	shaderMaterial->BindPSORuntimeModifyFunc([&]() {\
		RuntimePSOManager::Get().RegisterPSO(&shaderMaterial->GetComputePSO(), GET_SHADER_PATH(shaderPath), D3D12_SHVER_COMPUTE_SHADER);\
		});\
	shaderMaterial->IsInitialized = true;\
}

	if (g_bTypedUAVLoadSupport_R11G11B10_FLOAT)
	{
		InitPostProcessMaterial(ToneMapCS, "Tone Map CS", "PostProcessing/ToneMapping/ToneMap2CS.hlsl", g_pToneMap2CS);
		InitPostProcessMaterial(ToneMapHDRCS, "Tone Map HDR CS", "PostProcessing/ToneMapping/ToneMapHDR2CS.hlsl", g_pToneMapHDR2CS);
	}
	else
	{
		InitPostProcessMaterial(ToneMapCS, "Tone Map CS", "PostProcessing/ToneMapping/ToneMapCS.hlsl", g_pToneMapCS);
		InitPostProcessMaterial(ToneMapHDRCS, "Tone Map HDR CS", "PostProcessing/ToneMapping/ToneMapHDRCS.hlsl", g_pToneMapHDRCS);
	}

	InitPostProcessMaterial(GenerateLuminanceHistogramCS, "Generate Luminance Histogram CS", "PostProcessing/GenerateLuminanceHistogramCS.hlsl", g_pGenerateLuminanceHistogramCS);
	InitPostProcessMaterial(UpsampleBlurCS, "UpSample and Blur CS", "PostProcessing/UpsampleBlurCS.hlsl", g_pUpsampleBlurCS);
	InitPostProcessMaterial(BlurCS, "Blur CS", "PostProcessing/BlurCS.hlsl", g_pBlurCS);

	InitPostProcessMaterial(DownsampleBloom2CS, "DownSample Bloom 2 CS", "PostProcessing/Bloom/BloomDownSample2CS.hlsl", g_pBloomDownSample2CS);
	InitPostProcessMaterial(DownsampleBloom4CS, "DownSample Bloom 4 CS", "PostProcessing/Bloom/BloomDownSample4CS.hlsl", g_pBloomDownSample4CS);
	InitPostProcessMaterial(BloomExtractAndDownsampleHDRCS, "Bloom Extract and DownSample HDR CS", "PostProcessing/Bloom/BloomExtractAndDownSampleHDRCS.hlsl", g_pBloomExtractAndDownSampleHDRCS);

	InitPostProcessMaterial(ExtractLuminanceCS, "Extract Luminance CS", "PostProcessing/ExtractLuminanceCS.hlsl", g_pExtractLuminanceCS);
	InitPostProcessMaterial(CopyBackBufferForNotHDRUAVSupportCS, "Copy Back Post Buffer CS For Not HDR UAV Support", "PostProcessing/CopyPostBufferHDRCS.hlsl", g_pCopyPostBufferHDRCS);
	InitPostProcessMaterial(AdaptExposureCS, "Adapt Exposure CS", "PostProcessing/AdaptExposureCS.hlsl", g_pAdaptExposureCS);
	InitPostProcessMaterial(DrawHistogramCS, "Draw Histogram CS", "PostProcessing/DrawHistogramCS.hlsl", g_pDrawHistogramCS);
	InitPostProcessMaterial(LinearizeDepthCS, "Linearize Depth CS", "Misc/LinearizeDepthCS.hlsl", g_pLinearizeDepthCS);

	InitPostProcessMaterial(BilateralBlurFloat1CS, "Bilateral Blur Float1 CS", "PostProcessing/Blur/BilateralBlurFloat1CS.hlsl", g_pBilateralBlurFloat1CS);
	InitPostProcessMaterial(BilateralBlurFloat3CS, "Bilateral Blur Float3 CS", "PostProcessing/Blur/BilateralBlurFloat3CS.hlsl", g_pBilateralBlurFloat3CS);
	InitPostProcessMaterial(BilateralBlurFloat4CS, "Bilateral Blur Float4 CS", "PostProcessing/Blur/BilateralBlurFloat4CS.hlsl", g_pBilateralBlurFloat4CS);
	InitPostProcessMaterial(BilateralBlurUnorm1CS, "Bilateral Blur Unorm1 CS", "PostProcessing/Blur/BilateralBlurUnorm1CS.hlsl", g_pBilateralBlurUnorm1CS);
	InitPostProcessMaterial(BilateralBlurUnorm4CS, "Bilateral Blur Unorm4 CS", "PostProcessing/Blur/BilateralBlurUnorm4CS.hlsl", g_pBilateralBlurUnorm4CS);
	InitPostProcessMaterial(BilateralBlurWideFloat1CS, "Bilateral Blur Wide Float1 CS", "PostProcessing/Blur/BilateralBlurWideFloat1CS.hlsl", g_pBilateralBlurWideFloat1CS);
	InitPostProcessMaterial(BilateralBlurWideFloat3CS, "Bilateral Blur Wide Float3 CS", "PostProcessing/Blur/BilateralBlurWideFloat3CS.hlsl", g_pBilateralBlurWideFloat3CS);
	InitPostProcessMaterial(BilateralBlurWideFloat4CS, "Bilateral Blur Wide Float4 CS", "PostProcessing/Blur/BilateralBlurWideFloat4CS.hlsl", g_pBilateralBlurWideFloat4CS);
	InitPostProcessMaterial(BilateralBlurWideUnorm1CS, "Bilateral Blur Wide Unorm1 CS", "PostProcessing/Blur/BilateralBlurWideUnorm1CS.hlsl", g_pBilateralBlurWideUnorm1CS);
	InitPostProcessMaterial(BilateralBlurWideUnorm4CS, "Bilateral Blur Wide Unorm4 CS", "PostProcessing/Blur/BilateralBlurWideUnorm4CS.hlsl", g_pBilateralBlurWideUnorm4CS);
	InitPostProcessMaterial(GaussianBlurFloat1CS, "Gaussian Blur Float1 CS", "PostProcessing/Blur/GaussianBlurFloat1CS.hlsl", g_pGaussianBlurFloat1CS);
	InitPostProcessMaterial(GaussianBlurFloat3CS, "Gaussian Blur Float3 CS", "PostProcessing/Blur/GaussianBlurFloat3CS.hlsl", g_pGaussianBlurFloat3CS);
	InitPostProcessMaterial(GaussianBlurFloat4CS, "Gaussian Blur Float4 CS", "PostProcessing/Blur/GaussianBlurFloat4CS.hlsl", g_pGaussianBlurFloat4CS);
	InitPostProcessMaterial(GaussianBlurUnorm1CS, "Gaussian Blur Unorm1 CS", "PostProcessing/Blur/GaussianBlurUnorm1CS.hlsl", g_pGaussianBlurUnorm1CS);
	InitPostProcessMaterial(GaussianBlurUnorm4CS, "Gaussian Blur Unorm4 CS", "PostProcessing/Blur/GaussianBlurUnorm4CS.hlsl", g_pGaussianBlurUnorm4CS);
	InitPostProcessMaterial(GaussianBlurWideFloat1CS, "Gaussian Blur Wide Float1 CS", "PostProcessing/Blur/GaussianBlurWideFloat1CS.hlsl", g_pGaussianBlurWideFloat1CS);
	InitPostProcessMaterial(GaussianBlurWideFloat3CS, "Gaussian Blur Wide Float3 CS", "PostProcessing/Blur/GaussianBlurWideFloat3CS.hlsl", g_pGaussianBlurWideFloat3CS);
	InitPostProcessMaterial(GaussianBlurWideFloat4CS, "Gaussian Blur Wide Float4 CS", "PostProcessing/Blur/GaussianBlurWideFloat4CS.hlsl", g_pGaussianBlurWideFloat4CS);
	InitPostProcessMaterial(GaussianBlurWideUnorm1CS, "Gaussian Blur Wide Unorm1 CS", "PostProcessing/Blur/GaussianBlurWideUnorm1CS.hlsl", g_pGaussianBlurWideUnorm1CS);
	InitPostProcessMaterial(GaussianBlurWideUnorm4CS, "Gaussian Blur Wide Unorm4 CS", "PostProcessing/Blur/GaussianBlurWideUnorm4CS.hlsl", g_pGaussianBlurWideUnorm4CS);

#undef CreatePSO

	ShaderMaterials[BilateralBlurFloat1] = BilateralBlurFloat1CS;
	ShaderMaterials[BilateralBlurFloat3] = BilateralBlurFloat3CS;
	ShaderMaterials[BilateralBlurFloat4] = BilateralBlurFloat4CS;
	ShaderMaterials[BilateralBlurUnorm1] = BilateralBlurUnorm1CS;
	ShaderMaterials[BilateralBlurUnorm4] = BilateralBlurUnorm4CS;

	ShaderMaterials[BilateralBlurWideFloat1] = BilateralBlurWideFloat1CS;
	ShaderMaterials[BilateralBlurWideFloat3] = BilateralBlurWideFloat3CS;
	ShaderMaterials[BilateralBlurWideFloat4] = BilateralBlurWideFloat4CS;
	ShaderMaterials[BilateralBlurWideUnorm1] = BilateralBlurWideUnorm1CS;
	ShaderMaterials[BilateralBlurWideUnorm4] = BilateralBlurWideUnorm4CS;

	ShaderMaterials[GaussianBlurFloat1] = GaussianBlurFloat1CS;
	ShaderMaterials[GaussianBlurFloat3] = GaussianBlurFloat3CS;
	ShaderMaterials[GaussianBlurFloat4] = GaussianBlurFloat4CS;
	ShaderMaterials[GaussianBlurUnorm1] = GaussianBlurUnorm1CS;
	ShaderMaterials[GaussianBlurUnorm4] = GaussianBlurUnorm4CS;

	ShaderMaterials[GaussianBlurWideFloat1] = GaussianBlurWideFloat1CS;
	ShaderMaterials[GaussianBlurWideFloat3] = GaussianBlurWideFloat3CS;
	ShaderMaterials[GaussianBlurWideFloat4] = GaussianBlurWideFloat4CS;
	ShaderMaterials[GaussianBlurWideUnorm1] = GaussianBlurWideUnorm1CS;
	ShaderMaterials[GaussianBlurWideUnorm4] = GaussianBlurWideUnorm4CS;
}


void ScreenProcessing::Render(const Camera& camera)
{
	ComputeContext& Context = ComputeContext::Begin(L"Post Processing");

	LastPostprocessRT = &g_PostColorBuffer;
	CurrentPostprocessRT = &g_SceneColorBuffer;

	Context.SetRootSignature(PostEffectsRS);

	Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

	//Some systems generate a per-pixel velocity buffer to better track dynamic and skinned meshes.  Everything
	//is static in our scene, so we generate velocity from camera motion and the depth buffer.  A velocity buffer
	//is necessary for all temporal effects (and motion blur).
	MotionBlur::GenerateCameraVelocityBuffer(Context, camera);


	if (Render::GetAntiAliasingType() == Render::AntiAliasingType::TAA ||
		Render::GetAntiAliasingType() == Render::AntiAliasingType::TFAA)
	{
		TemporalAA::ApplyTemporalAA(Context, *CurrentPostprocessRT);
	}

	if (!EngineGlobal::gCurrentScene->IsWireFrame)
	{

		if (BloomEnable)
		{
			GenerateBloom(Context);
			Context.TransitionResource(g_aBloomUAV1[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}
		else if (EnableAdaptation)
		{
			ExtractLuminance(Context);
		}

		if (EnableEyeAdaptation)
		{
			//Luminance Adaptation
			Adaptation(Context);
		}

		//ToneMapping
		ToneMappingHDR(Context);

		std::swap(LastPostprocessRT, CurrentPostprocessRT);

		//Motion Blur
		MotionBlur::RenderMotionBlur(Context, g_VelocityBuffer, LastPostprocessRT);
	}
	else
	{
		std::swap(LastPostprocessRT, CurrentPostprocessRT);
	}

	if ((Render::GetAntiAliasingType() == Render::AntiAliasingType::FXAA && FXAA::IsEnable) ||
		Render::GetAntiAliasingType() == Render::AntiAliasingType::TFAA)
	{
		FXAA::Render(Context, LastPostprocessRT, CurrentPostprocessRT);
		std::swap(LastPostprocessRT, CurrentPostprocessRT);
	}

	if (DrawHistogram)
	{
		ScopedTimer Scope(L"Draw Debug Histogram", Context);
		Context.SetRootSignature(PostEffectsRS);
		Context.SetPipelineState(DrawHistogramCS->GetRuntimePSO());
		Context.InsertUAVBarrier(*LastPostprocessRT);
		Context.TransitionResource(g_Histogram, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_Exposure, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.SetDynamicDescriptor(1, 0, LastPostprocessRT->GetUAV());
		D3D12_CPU_DESCRIPTOR_HANDLE SRVs[2] = { g_Histogram.GetSRV(), g_Exposure.GetSRV() };
		Context.SetDynamicDescriptors(2, 0, 2, SRVs);
		Context.Dispatch(1, 32);
		Context.TransitionResource(*LastPostprocessRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	if (Render::GetAntiAliasingType() == Render::AntiAliasingType::FSR)
	{
		//Execute FSR 
		Context.TransitionResource(g_SceneFullScreenBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE| D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,true);
		FSR::GetInstance()->Execute(Context, *LastPostprocessRT, g_SceneFullScreenBuffer);
	}
	
	//if (!g_bTypedUAVLoadSupport_R11G11B10_FLOAT)
	//{
	//	CopyBackBufferForNotHDRUAVSupport(Context);
	//}

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
	Context.Finish();
}

void ScreenProcessing::DrawAfterToneMapping()
{
	GraphicsContext& Context = GraphicsContext::Begin(L"After Tone Mapping");
	//FBM
	//Context.SetDynamicConstantBufferView((int)RootSignatureType::eMainConstantBuffer, sizeof(gMainConstants), &gMainConstants);
	//RenderFBM(Context);

	Context.Finish();
}

void ScreenProcessing::DrawUI()
{
	ImGuiIO& io = ImGui::GetIO();

	if (ImGui::CollapsingHeader("Anti-aliasing", ImGuiTreeNodeFlags_DefaultOpen))
	{
		mPreAntiAliasingIndex = mAntiAliasingIndex;

		ImGui::Combo("Anti Aliasing", &mAntiAliasingIndex, mAntiAliasingName.c_str());

		if (Render::gRasterRenderPipelineType == RasterRenderPipelineType::TBFR)
		{
			mAntiAliasingIndex = (int)Render::AntiAliasingType::TAA;
			Render::SetAntiAliasingType(Render::AntiAliasingType(mAntiAliasingIndex));
		}
		else
		{
			Render::SetAntiAliasingType(Render::AntiAliasingType(mAntiAliasingIndex));
		}

		switch (mAntiAliasingIndex)
		{
		case Render::AntiAliasingType::MSAA:
			break;
		case Render::AntiAliasingType::FXAA:
		{
			if (ImGui::TreeNodeEx("FXAA"))
			{
				FXAA::DrawUI();
				ImGui::TreePop();
			}
			break;
		}
		case Render::AntiAliasingType::TAA:
		{
			if (ImGui::TreeNodeEx("TAA"))
			{
				TemporalAA::DrawUI();
				ImGui::TreePop();
			}
			break;
		}
		case Render::AntiAliasingType::TFAA:
		{
			if (ImGui::TreeNodeEx("TFAA"))
			{
				TemporalAA::DrawUI();
				ImGui::TreePop();
			}
			break;
		}
		default:
			break;
		}
	}

	if (ImGui::CollapsingHeader("Post Processing", ImGuiTreeNodeFlags_None))
	{
		if (ImGui::TreeNodeEx("SSAO", ImGuiTreeNodeFlags_DefaultOpen))
		{
			SSAO::DrawUI();
			ImGui::TreePop();
		}

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

		if (ImGui::TreeNodeEx("Motion Blur", ImGuiTreeNodeFlags_DefaultOpen))
		{
			MotionBlur::DrawUI();
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Luminance", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Enable Adaption", &EnableEyeAdaptation);
			ImGui::SliderVerticalFloat("Target Luminance:", &TargetLuminance.GetValue(), TargetLuminance.GetMinValue(), TargetLuminance.GetMaxValue());
			ImGui::Checkbox("Draw Luminance Histogram", &DrawHistogram);
			ImGui::TreePop();
		}
	}
}

void ScreenProcessing::Update(float dt, MainConstants& RenderData, Camera& camera)
{
	gMainConstants = &RenderData;

	//update camera jitter
	if (Render::GetAntiAliasingType() == Render::AntiAliasingType::TAA ||
		Render::GetAntiAliasingType() == Render::AntiAliasingType::TFAA)
	{
		TemporalAA::Update(Display::GetFrameCount());

		camera.UpdateJitter(TemporalAA::GetJitterOffset());
	}
	else if (Render::GetAntiAliasingType() == Render::AntiAliasingType::FSR)
	{
		camera.UpdateJitter(FSR::GetInstance()->GetFSRjitter());
	}
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
	Context.SetPipelineState(UpsampleBlurCS->GetRuntimePSO());

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
	Context.SetPipelineState(BlurCS->GetRuntimePSO());

	// Dispatch the compute shader with default 8x8 thread groups
	Context.Dispatch2D(bufferWidth, bufferHeight);

	Context.TransitionResource(TargetBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void ScreenProcessing::GaussianBlur(ComputeContext& Context, ColorBuffer& SourceBuffer, ColorBuffer& TempBuffer, bool IsWideBlur)
{
	assert(gMainConstants);

	BlurConstants ConstantData =
	{
		XMFLOAT2(1.0f / SourceBuffer.GetWidth(),1.0f / SourceBuffer.GetHeight()),
		XMINT2(SourceBuffer.GetWidth(),SourceBuffer.GetHeight()),
		true,
		gMainConstants->FarZ
	};

	Context.TransitionResource(TempBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.SetDynamicDescriptor(1, 0, TempBuffer.GetUAV());
	Context.SetDynamicDescriptor(2, 0, SourceBuffer.GetSRV());

	D3D12_CPU_DESCRIPTOR_HANDLE HorizontalSRVs[2] = { SourceBuffer.GetSRV(),CurrentLinearDepth->GetSRV() };
	Context.SetDynamicDescriptors(2, 0, 2, HorizontalSRVs);

	Context.SetDynamicConstantBufferView(3, sizeof(BlurConstants), &ConstantData);

	BlurShaderType GaussianBlurShaderIndex;
	switch (SourceBuffer.GetFormat())
	{
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R8_UNORM:
		GaussianBlurShaderIndex = IsWideBlur ? GaussianBlurWideUnorm1 : GaussianBlurUnorm1;
		break;
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
		GaussianBlurShaderIndex = IsWideBlur ? GaussianBlurWideFloat1 : GaussianBlurFloat1;
		break;
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UNORM:
		GaussianBlurShaderIndex = IsWideBlur ? GaussianBlurWideUnorm4 : GaussianBlurUnorm4;
		break;
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_R32G32B32_FLOAT:
		GaussianBlurShaderIndex = IsWideBlur ? GaussianBlurWideFloat3 : GaussianBlurFloat3;
		break;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
		GaussianBlurShaderIndex = IsWideBlur ? GaussianBlurWideFloat4 : GaussianBlurFloat4;
		break;
	default:
		assert(0); // implement format!
		break;
	}

	Context.SetPipelineState(ShaderMaterials[GaussianBlurShaderIndex]->GetRuntimePSO());

	//Horizontal Blur
	Context.Dispatch2D(ConstantData.Dimensions.x, ConstantData.Dimensions.y, 256, 1);

	ConstantData.IsHorizontalBlur = false;

	Context.SetDynamicConstantBufferView(3, sizeof(BlurConstants), &ConstantData);

	Context.SetDynamicDescriptor(1, 0, SourceBuffer.GetUAV());
	Context.SetDynamicDescriptor(2, 0, TempBuffer.GetSRV());

	D3D12_CPU_DESCRIPTOR_HANDLE VerticalSRVs[2] = { TempBuffer.GetSRV(),CurrentLinearDepth->GetSRV() };
	Context.SetDynamicDescriptors(2, 0, 2, VerticalSRVs);

	//Vertical Blur
	Context.Dispatch2D(ConstantData.Dimensions.x, ConstantData.Dimensions.y, 1, 256);

}

void ScreenProcessing::BilateralBlur(ComputeContext& Context, ColorBuffer& SourceBuffer, ColorBuffer& TempBuffer, bool IsWideBlur)
{
	assert(gMainConstants);

	BlurConstants ConstantData =
	{
		XMFLOAT2(1.0f / SourceBuffer.GetWidth(),1.0f / SourceBuffer.GetHeight()),
		XMINT2(SourceBuffer.GetWidth(),SourceBuffer.GetHeight()),
		true,
		gMainConstants->FarZ
	};

	Context.TransitionResource(TempBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.SetDynamicDescriptor(1, 0, TempBuffer.GetUAV());
	Context.SetDynamicDescriptor(2, 0, SourceBuffer.GetSRV());

	D3D12_CPU_DESCRIPTOR_HANDLE HorizontalSRVs[2] = { SourceBuffer.GetSRV(),CurrentLinearDepth->GetSRV() };
	Context.SetDynamicDescriptors(2, 0, 2, HorizontalSRVs);

	Context.SetDynamicConstantBufferView(3, sizeof(BlurConstants), &ConstantData);

	BlurShaderType BilateralBlurShaderIndex;
	switch (SourceBuffer.GetFormat())
	{
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R8_UNORM:
		BilateralBlurShaderIndex = IsWideBlur ? BilateralBlurWideUnorm1 : BilateralBlurUnorm1;
		break;
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
		BilateralBlurShaderIndex = IsWideBlur ? BilateralBlurWideFloat1 : BilateralBlurFloat1;
		break;
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UNORM:
		BilateralBlurShaderIndex = IsWideBlur ? BilateralBlurWideUnorm4 : BilateralBlurUnorm4;
		break;
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_R32G32B32_FLOAT:
		BilateralBlurShaderIndex = IsWideBlur ? BilateralBlurWideFloat3 : BilateralBlurFloat3;
		break;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
		BilateralBlurShaderIndex = IsWideBlur ? BilateralBlurWideFloat4 : BilateralBlurFloat4;
		break;
	default:
		assert(0); // implement format!
		break;
	}

	Context.SetPipelineState(ShaderMaterials[BilateralBlurShaderIndex]->GetRuntimePSO());

	//Horizontal Blur
	Context.Dispatch2D(ConstantData.Dimensions.x, ConstantData.Dimensions.y, 256, 1);

	ConstantData.IsHorizontalBlur = false;

	Context.SetDynamicConstantBufferView(3, sizeof(BlurConstants), &ConstantData);

	Context.SetDynamicDescriptor(1, 0, SourceBuffer.GetUAV());
	Context.SetDynamicDescriptor(2, 0, TempBuffer.GetSRV());

	D3D12_CPU_DESCRIPTOR_HANDLE VerticalSRVs[2] = { TempBuffer.GetSRV(),CurrentLinearDepth->GetSRV() };
	Context.SetDynamicDescriptors(2, 0, 2, VerticalSRVs);

	//Vertical Blur
	Context.Dispatch2D(ConstantData.Dimensions.x, ConstantData.Dimensions.y, 1, 256);
}

void ScreenProcessing::LinearizeZ(ComputeContext& Context, Camera& camera, uint32_t FrameIndex)
{
	DepthBuffer& Depth = g_SceneDepthBuffer;

	CurrentLinearDepth = &g_LinearDepth[FrameIndex];

	const float NearClipDist = camera.GetNearZ();

	const float FarClipDist = camera.GetFarZ();

	const float zMagic = (FarClipDist - NearClipDist) / NearClipDist;

	LinearizeZ(Context, Depth, CurrentLinearDepth, zMagic);
}

void ScreenProcessing::LinearizeZ(ComputeContext& Context, DepthBuffer& Depth, ColorBuffer* linearDepth, float zMagic)
{
	assert(linearDepth);

	ColorBuffer& LinearDepth = *linearDepth;

	// zMagic= (zFar - zNear) / zNear
	Context.SetRootSignature(ScreenProcessing::GetRootSignature());

	Context.TransitionResource(Depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	Context.SetConstants(0, zMagic);

	Context.SetDynamicDescriptor(2, 0, Depth.GetDepthSRV());

	Context.TransitionResource(LinearDepth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	Context.SetDynamicDescriptors(1, 0, 1, &LinearDepth.GetUAV());

	Context.SetPipelineState(LinearizeDepthCS->GetRuntimePSO());

	Context.Dispatch2D(LinearDepth.GetWidth(), LinearDepth.GetHeight(), 16, 16);

#ifdef DEBUG
	EngineGUI::AddRenderPassVisualizeTexture("Linear Z", WStringToString(g_LinearDepth[0].GetName()), g_LinearDepth[0].GetHeight(), g_LinearDepth[0].GetWidth(), g_LinearDepth[0].GetSRV());
#endif // DEBUG
}

bool ScreenProcessing::IsMSAA()
{
	return mAntiAliasingIndex == Render::AntiAliasingType::MSAA;
}

ColorBuffer* ScreenProcessing::GetLinearDepthBuffer()
{
	assert(CurrentLinearDepth);
	return CurrentLinearDepth;
}


void ScreenProcessing::ShutDown()
{
	g_Exposure.Destroy();
}

const RootSignature& ScreenProcessing::GetRootSignature()
{
	return PostEffectsRS;
}

ColorBuffer* ScreenProcessing::GetCurrentPostprocessRT()
{
	return CurrentPostprocessRT;
}

ColorBuffer* ScreenProcessing::GetLastPostprocessRT()
{
	return LastPostprocessRT;
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
	Context.SetPipelineState(BloomExtractAndDownsampleHDRCS->GetRuntimePSO());
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
		Context.SetPipelineState(DownsampleBloom4CS->GetRuntimePSO());
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

		Context.SetPipelineState(DownsampleBloom2CS->GetRuntimePSO());
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
	Context.SetPipelineState(ExtractLuminanceCS->GetRuntimePSO());
	Context.Dispatch2D(g_LumaBloom.GetWidth(), g_LumaBloom.GetHeight());
}


void ScreenProcessing::CopyBackBufferForNotHDRUAVSupport(ComputeContext& Context)
{
	ScopedTimer Scope(L"Copy Post back to Scene For Not HDR UAV Support", Context);
	Context.SetRootSignature(PostEffectsRS);
	Context.SetPipelineState(CopyBackBufferForNotHDRUAVSupportCS->GetRuntimePSO());
	Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(g_PostEffectsBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.SetDynamicDescriptor(1, 0, g_SceneColorBuffer.GetUAV());
	Context.SetDynamicDescriptor(2, 0, g_PostEffectsBuffer.GetSRV());
	Context.Dispatch2D(g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight());
}