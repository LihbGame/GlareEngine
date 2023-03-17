#include "PostProcessing.h"
#include "Graphics/GraphicsCommon.h"
#include "Graphics/SamplerManager.h"
#include "Graphics/CommandSignature.h"


//shaders
#include "CompiledShaders/ScreenQuadVS.h"
#include "CompiledShaders/FbmPostPS.h"
#include "Engine/EngineProfiling.h"

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






	// Bloom effect
	void GenerateBloom(ComputeContext& Context);

}


void PostProcessing::Initialize(ID3D12GraphicsCommandList* CommandList)
{
	PostEffectsRS.Reset(4, 2);
	PostEffectsRS.InitStaticSampler(0, SamplerLinearClampDesc);
	PostEffectsRS.InitStaticSampler(1, SamplerLinearBorderDesc);
	PostEffectsRS[0].InitAsConstantBuffer(0);
	PostEffectsRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 4);
	PostEffectsRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4);
	PostEffectsRS[3].InitAsConstantBuffer(1);
	PostEffectsRS.Finalize(L"Post Effects");

	//Create Compute PSO Macro
#define CreatePSO( ObjName, ShaderByteCode ) \
    ObjName.SetRootSignature(PostEffectsRS); \
    ObjName.SetComputeShader(ShaderByteCode, sizeof(ShaderByteCode) ); \
    ObjName.Finalize();

	//g_bTypedUAVLoadSupport_R11G11B10_FLOAT



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
