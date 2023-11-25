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

	bool  BloomEnable				= true;
	bool  HighQualityBloom			= true;			// High quality blurs 5 octaves of bloom; low quality only blurs 3.
	NumVar BloomThreshold(4.0f, 0.0f, 8.0f);		// The threshold luminance above which a pixel will start to bloom
	NumVar BloomStrength(0.1f, 0.0f, 2.0f);			// A modulator controlling how much bloom is added back into the image
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
	int mAntiAliasingIndex = Render::AntiAliasingType::TAA;

	//Exposure Data
	StructuredBuffer g_Exposure;

	RootSignature PostEffectsRS;
	GraphicsPSO mPSO(L"FBM Post PS");

	MainConstants* gMainConstants = nullptr;

	ColorBuffer* CurrentLinearDepth = nullptr;

	ComputePSO* Shaders[BlurShaderType::Count];

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

	//Linear Depth PSO
	ComputePSO LinearizeDepthCS(L"Linearize Depth CS");


	//Blur PSO
	ComputePSO BilateralBlurFloat1CS(L"Bilateral Blur Float1 CS");
	ComputePSO BilateralBlurFloat3CS(L"Bilateral Blur Float3 CS");
	ComputePSO BilateralBlurFloat4CS(L"Bilateral Blur Float4 CS");
	ComputePSO BilateralBlurUnorm1CS(L"Bilateral Blur Unorm1 CS");
	ComputePSO BilateralBlurUnorm4CS(L"Bilateral Blur Unorm4 CS");
	ComputePSO BilateralBlurWideFloat1CS(L"Bilateral Blur Wide Float1 CS");
	ComputePSO BilateralBlurWideFloat3CS(L"Bilateral Blur Wide Float3 CS");
	ComputePSO BilateralBlurWideFloat4CS(L"Bilateral Blur Wide Float4 CS");
	ComputePSO BilateralBlurWideUnorm1CS(L"Bilateral Blur Wide Unorm1 CS");
	ComputePSO BilateralBlurWideUnorm4CS(L"Bilateral Blur Wide Unorm4 CS");
	ComputePSO GaussianBlurFloat1CS(L"Gaussian Blur Float1 CS");
	ComputePSO GaussianBlurFloat3CS(L"Gaussian Blur Float3 CS");
	ComputePSO GaussianBlurFloat4CS(L"Gaussian Blur Float4 CS");
	ComputePSO GaussianBlurUnorm1CS(L"Gaussian Blur Unorm1 CS");
	ComputePSO GaussianBlurUnorm4CS(L"Gaussian Blur Unorm4 CS");
	ComputePSO GaussianBlurWideFloat1CS(L"Gaussian Blur Wide Float1 CS");
	ComputePSO GaussianBlurWideFloat3CS(L"Gaussian Blur Wide Float3 CS");
	ComputePSO GaussianBlurWideFloat4CS(L"Gaussian Blur Wide Float4 CS");
	ComputePSO GaussianBlurWideUnorm1CS(L"Gaussian Blur Wide Unorm1 CS");
	ComputePSO GaussianBlurWideUnorm4CS(L"Gaussian Blur Wide Unorm4 CS");

	void GenerateBloom(ComputeContext& Context);
	void ExtractLuminance(ComputeContext& Context);
	void CopyBackBufferForNotHDRUAVSupport(ComputeContext& Context);
	void ToneMappingHDR(ComputeContext& Context);
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
	CreatePSO(LinearizeDepthCS, g_pLinearizeDepthCS);

	CreatePSO(BilateralBlurFloat1CS, g_pBilateralBlurFloat1CS);
	CreatePSO(BilateralBlurFloat3CS, g_pBilateralBlurFloat3CS);
	CreatePSO(BilateralBlurFloat4CS, g_pBilateralBlurFloat4CS);
	CreatePSO(BilateralBlurUnorm1CS, g_pBilateralBlurUnorm1CS);
	CreatePSO(BilateralBlurUnorm4CS, g_pBilateralBlurUnorm4CS);
	CreatePSO(BilateralBlurWideFloat1CS, g_pBilateralBlurWideFloat1CS);
	CreatePSO(BilateralBlurWideFloat3CS, g_pBilateralBlurWideFloat3CS);
	CreatePSO(BilateralBlurWideFloat4CS, g_pBilateralBlurWideFloat4CS);
	CreatePSO(BilateralBlurWideUnorm1CS, g_pBilateralBlurWideUnorm1CS);
	CreatePSO(BilateralBlurWideUnorm4CS, g_pBilateralBlurWideUnorm4CS);
	CreatePSO(GaussianBlurFloat1CS, g_pGaussianBlurFloat1CS);
	CreatePSO(GaussianBlurFloat3CS, g_pGaussianBlurFloat3CS);
	CreatePSO(GaussianBlurFloat4CS, g_pGaussianBlurFloat4CS);
	CreatePSO(GaussianBlurUnorm1CS, g_pGaussianBlurUnorm1CS);
	CreatePSO(GaussianBlurUnorm4CS, g_pGaussianBlurUnorm4CS);
	CreatePSO(GaussianBlurWideFloat1CS, g_pGaussianBlurWideFloat1CS);
	CreatePSO(GaussianBlurWideFloat3CS, g_pGaussianBlurWideFloat3CS);
	CreatePSO(GaussianBlurWideFloat4CS, g_pGaussianBlurWideFloat4CS);
	CreatePSO(GaussianBlurWideUnorm1CS, g_pGaussianBlurWideUnorm1CS);
	CreatePSO(GaussianBlurWideUnorm4CS, g_pGaussianBlurWideUnorm4CS);

#undef CreatePSO

	Shaders[BilateralBlurFloat1] = &BilateralBlurFloat1CS;
	Shaders[BilateralBlurFloat3] = &BilateralBlurFloat3CS;
	Shaders[BilateralBlurFloat4] = &BilateralBlurFloat4CS;
	Shaders[BilateralBlurUnorm1] = &BilateralBlurUnorm1CS;
	Shaders[BilateralBlurUnorm4] = &BilateralBlurUnorm4CS;

	Shaders[BilateralBlurWideFloat1] = &BilateralBlurWideFloat1CS;
	Shaders[BilateralBlurWideFloat3] = &BilateralBlurWideFloat3CS;
	Shaders[BilateralBlurWideFloat4] = &BilateralBlurWideFloat4CS;
	Shaders[BilateralBlurWideUnorm1] = &BilateralBlurWideUnorm1CS;
	Shaders[BilateralBlurWideUnorm4] = &BilateralBlurWideUnorm4CS;

	Shaders[GaussianBlurFloat1] = &GaussianBlurFloat1CS;
	Shaders[GaussianBlurFloat3] = &GaussianBlurFloat3CS;
	Shaders[GaussianBlurFloat4] = &GaussianBlurFloat4CS;
	Shaders[GaussianBlurUnorm1] = &GaussianBlurUnorm1CS;
	Shaders[GaussianBlurUnorm4] = &GaussianBlurUnorm4CS;

	Shaders[GaussianBlurWideFloat1] = &GaussianBlurWideFloat1CS;
	Shaders[GaussianBlurWideFloat3] = &GaussianBlurWideFloat3CS;
	Shaders[GaussianBlurWideFloat4] = &GaussianBlurWideFloat4CS;
	Shaders[GaussianBlurWideUnorm1] = &GaussianBlurWideUnorm1CS;
	Shaders[GaussianBlurWideUnorm4] = &GaussianBlurWideUnorm4CS;

	mAntiAliasingName += string("MSAA") + '\0';
	mAntiAliasingName += string("FXAA") + '\0';
	mAntiAliasingName += string("TAA") + '\0';
	mAntiAliasingName += string("NoAA") + '\0';

	__declspec(align(16)) float initExposure[] =
	{
		Exposure, 1.0f / Exposure,InitialMinLog, InitialMaxLog, InitialMaxLog - InitialMinLog, 1.0f / (InitialMaxLog - InitialMinLog),0.0f
	};
	g_Exposure.Create(L"Exposure", 7, 4, initExposure);

	BuildSRV(CommandList);

	//FXAA Initialize
	FXAA::Initialize();

	//Motion Blur Initialize
	MotionBlur::Initialize();

	//TemporalAA Initialize
	TemporalAA::Initialize();

#if	USE_RUNTIME_PSO
	RuntimePSOManager::Get().RegisterPSO(&ToneMapCS, GET_SHADER_PATH("PostProcessing/ToneMapping/ToneMap2CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&ToneMapHDRCS, GET_SHADER_PATH("PostProcessing/ToneMapping/ToneMapHDR2CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&GenerateLuminanceHistogramCS, GET_SHADER_PATH("PostProcessing/GenerateLuminanceHistogramCS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&UpsampleBlurCS, GET_SHADER_PATH("PostProcessing/UpsampleBlurCS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&BlurCS, GET_SHADER_PATH("PostProcessing/BlurCS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&DownsampleBloom2CS, GET_SHADER_PATH("PostProcessing/Bloom/BloomDownSample2CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&DownsampleBloom4CS, GET_SHADER_PATH("PostProcessing/Bloom/BloomDownSample4CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&BloomExtractAndDownsampleHDRCS, GET_SHADER_PATH("PostProcessing/Bloom/BloomExtractAndDownSampleHDRCS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&ExtractLuminanceCS, GET_SHADER_PATH("PostProcessing/ExtractLuminanceCS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&CopyBackBufferForNotHDRUAVSupportCS, GET_SHADER_PATH("PostProcessing/CopyPostBufferHDRCS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&AdaptExposureCS, GET_SHADER_PATH("PostProcessing/AdaptExposureCS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&DrawHistogramCS, GET_SHADER_PATH("PostProcessing/DrawHistogramCS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&LinearizeDepthCS, GET_SHADER_PATH("Misc/LinearizeDepthCS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	
	RuntimePSOManager::Get().RegisterPSO(&BilateralBlurFloat1CS, GET_SHADER_PATH("PostProcessing/Blur/BilateralBlurFloat1CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&BilateralBlurFloat3CS, GET_SHADER_PATH("PostProcessing/Blur/BilateralBlurFloat3CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&BilateralBlurFloat4CS, GET_SHADER_PATH("PostProcessing/Blur/BilateralBlurFloat4CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&BilateralBlurUnorm1CS, GET_SHADER_PATH("PostProcessing/Blur/BilateralBlurUnorm1CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&BilateralBlurUnorm4CS, GET_SHADER_PATH("PostProcessing/Blur/BilateralBlurUnorm4CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&BilateralBlurWideFloat1CS, GET_SHADER_PATH("PostProcessing/Blur/BilateralBlurWideFloat1CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&BilateralBlurWideFloat3CS, GET_SHADER_PATH("PostProcessing/Blur/BilateralBlurWideFloat3CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&BilateralBlurWideFloat4CS, GET_SHADER_PATH("PostProcessing/Blur/BilateralBlurWideFloat4CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&BilateralBlurWideUnorm1CS, GET_SHADER_PATH("PostProcessing/Blur/BilateralBlurWideUnorm1CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&BilateralBlurWideUnorm4CS, GET_SHADER_PATH("PostProcessing/Blur/BilateralBlurWideUnorm4CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&GaussianBlurFloat1CS, GET_SHADER_PATH("PostProcessing/Blur/GaussianBlurFloat1CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&GaussianBlurFloat3CS, GET_SHADER_PATH("PostProcessing/Blur/GaussianBlurFloat3CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&GaussianBlurFloat4CS, GET_SHADER_PATH("PostProcessing/Blur/GaussianBlurFloat4CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&GaussianBlurUnorm1CS, GET_SHADER_PATH("PostProcessing/Blur/GaussianBlurUnorm1CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&GaussianBlurUnorm4CS, GET_SHADER_PATH("PostProcessing/Blur/GaussianBlurUnorm4CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&GaussianBlurWideFloat1CS, GET_SHADER_PATH("PostProcessing/Blur/GaussianBlurWideFloat1CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&GaussianBlurWideFloat3CS, GET_SHADER_PATH("PostProcessing/Blur/GaussianBlurWideFloat3CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&GaussianBlurWideFloat4CS, GET_SHADER_PATH("PostProcessing/Blur/GaussianBlurWideFloat4CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&GaussianBlurWideUnorm1CS, GET_SHADER_PATH("PostProcessing/Blur/GaussianBlurWideUnorm1CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&GaussianBlurWideUnorm4CS, GET_SHADER_PATH("PostProcessing/Blur/GaussianBlurWideUnorm4CS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
#endif

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
		Context.SetPipelineState(GET_PSO(mPSO));
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

	Context.SetPipelineState(Display::g_bEnableHDROutput ? GET_PSO(ToneMapHDRCS) : GET_PSO(ToneMapCS));

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
	Context.SetPipelineState(GET_PSO(GenerateLuminanceHistogramCS));
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
	Context.SetPipelineState(GET_PSO(AdaptExposureCS));
	Context.Dispatch();
	Context.TransitionResource(g_Exposure, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}


void ScreenProcessing::Render(const Camera& camera)
{
	ComputeContext& Context = ComputeContext::Begin(L"Post Processing");

	LastPostprocessRT = &g_PostColorBuffer;
	CurrentPostprocessRT= &g_SceneColorBuffer;

	Context.SetRootSignature(PostEffectsRS);

	Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

	//Some systems generate a per-pixel velocity buffer to better track dynamic and skinned meshes.  Everything
	//is static in our scene, so we generate velocity from camera motion and the depth buffer.  A velocity buffer
	//is necessary for all temporal effects (and motion blur).
	MotionBlur::GenerateCameraVelocityBuffer(Context, camera);

	if (Render::GetAntiAliasingType() == Render::AntiAliasingType::TAA)
	{
		TemporalAA::ApplyTemporalAA(Context);
	}

	if (BloomEnable)
	{
		GenerateBloom(Context);
		Context.TransitionResource(g_aBloomUAV1[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}
	else if (EnableAdaptation)
	{
		ExtractLuminance(Context);
	}

	//Luminance Adaptation
	Adaptation(Context);

	//ToneMapping
	ToneMappingHDR(Context);

	std::swap(LastPostprocessRT, CurrentPostprocessRT);

	//Motion Blur
	MotionBlur::RenderMotionBlur(Context, g_VelocityBuffer, LastPostprocessRT);

	if (Render::GetAntiAliasingType() == Render::AntiAliasingType::FXAA && FXAA::IsEnable)
	{
		FXAA::Render(Context, LastPostprocessRT, CurrentPostprocessRT);
		std::swap(LastPostprocessRT, CurrentPostprocessRT);
	}

	//if (!g_bTypedUAVLoadSupport_R11G11B10_FLOAT)
	//{
	//	CopyBackBufferForNotHDRUAVSupport(Context);
	//}

	if (DrawHistogram)
	{
		ScopedTimer Scope(L"Draw Debug Histogram", Context);
		Context.SetRootSignature(PostEffectsRS);
		Context.SetPipelineState(GET_PSO(DrawHistogramCS));
		Context.InsertUAVBarrier(*LastPostprocessRT);
		Context.TransitionResource(g_Histogram, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_Exposure, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.SetDynamicDescriptor(1, 0, LastPostprocessRT->GetUAV());
		D3D12_CPU_DESCRIPTOR_HANDLE SRVs[2] = { g_Histogram.GetSRV(), g_Exposure.GetSRV() };
		Context.SetDynamicDescriptors(2, 0, 2, SRVs);
		Context.Dispatch(1, 32);
		Context.TransitionResource(*LastPostprocessRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
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

	if (ImGui::CollapsingHeader("Anti-aliasing", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Combo("Anti Aliasing", &mAntiAliasingIndex, mAntiAliasingName.c_str());

		Render::SetAntiAliasingType(Render::AntiAliasingType(mAntiAliasingIndex));

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
				ImGui::TreePop();
			}
			break;
		}
		default:
			break;
		}
	}

	if (ImGui::CollapsingHeader("Post Processing", ImGuiTreeNodeFlags_DefaultOpen))
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
	if (Render::GetAntiAliasingType() == Render::AntiAliasingType::TAA)
	{
		TemporalAA::Update(Display::GetFrameCount());

		camera.UpdateJitter(TemporalAA::GetJitterOffset());
	}
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
	Context.SetPipelineState(GET_PSO(UpsampleBlurCS));

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
	Context.SetPipelineState(GET_PSO(BlurCS));

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

	ComputePSO& BlurPSO = *Shaders[GaussianBlurShaderIndex];
	Context.SetPipelineState(GET_PSO(BlurPSO));

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

	ComputePSO& BlurPSO = *Shaders[BilateralBlurShaderIndex];
	Context.SetPipelineState(BlurPSO);

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

	CurrentLinearDepth = &g_LinearDepth;

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

	Context.SetPipelineState(GET_PSO(LinearizeDepthCS));

	Context.Dispatch2D(LinearDepth.GetWidth(), LinearDepth.GetHeight(), 16, 16);

#ifdef DEBUG
	EngineGUI::AddRenderPassVisualizeTexture("Linear Z", WStringToString(LinearDepth.GetName()), LinearDepth.GetHeight(), LinearDepth.GetWidth(), LinearDepth.GetSRV());
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
	Context.SetPipelineState(GET_PSO(BloomExtractAndDownsampleHDRCS));
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
		Context.SetPipelineState(GET_PSO(DownsampleBloom4CS));
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

		Context.SetPipelineState(GET_PSO(DownsampleBloom2CS));
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
	Context.SetPipelineState(GET_PSO(ExtractLuminanceCS));
	Context.Dispatch2D(g_LumaBloom.GetWidth(), g_LumaBloom.GetHeight());
}


void ScreenProcessing::CopyBackBufferForNotHDRUAVSupport(ComputeContext& Context)
{
	ScopedTimer Scope(L"Copy Post back to Scene For Not HDR UAV Support", Context);
	Context.SetRootSignature(PostEffectsRS);
	Context.SetPipelineState(GET_PSO(CopyBackBufferForNotHDRUAVSupportCS));
	Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(g_PostEffectsBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.SetDynamicDescriptor(1, 0, g_SceneColorBuffer.GetUAV());
	Context.SetDynamicDescriptor(2, 0, g_PostEffectsBuffer.GetSRV());
	Context.Dispatch2D(g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight());
}