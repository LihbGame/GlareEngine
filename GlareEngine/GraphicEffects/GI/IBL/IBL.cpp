#include "IBL.h"
#include "Sky.h"

//shader
#include "CompiledShaders/BakingEnvDiffusePS.h"
#include "CompiledShaders/BakingEnvDiffuseVS.h"
#include "CompiledShaders/PreFilteredEnvironmentMapPS.h"

#define  MaxMipLevels 5


GraphicsPSO IBL::mIndirectDiffusePSO;
GraphicsPSO IBL::mPreFilteredEnvMapPSO;
RootSignature* IBL::m_pRootSignature = nullptr;


IBL::IBL()
{
}

IBL::~IBL()
{
}


void IBL::Initialize()
{
	mIndirectDiffuseCube = make_unique<CubeRenderTarget>(BAKECUBESIZE, BAKECUBESIZE);
	mPreFilteredEnvCube = make_unique<CubeRenderTarget>(BAKECUBESIZE / 2, BAKECUBESIZE / 2, MaxMipLevels);

	GlobleSRVIndex::mBakingDiffuseCubeIndex = mIndirectDiffuseCube->GetSRVIndex();
}

void IBL::BakingEnvironmentDiffuse(GraphicsContext& Context)
{
	assert(m_pSky);
	assert(m_pRootSignature);
	Context.PIXBeginEvent(L"Baking Environment Diffuse");
	Context.SetRootSignature(*m_pRootSignature);
	Context.SetViewport(mIndirectDiffuseCube->Viewport());
	Context.SetScissor(mIndirectDiffuseCube->ScissorRect());
	Context.TransitionResource(mIndirectDiffuseCube->Resource(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	Context.SetDynamicDescriptors(2, 0, (UINT)g_CubeSRV.size(), g_CubeSRV.data());
	for (int i = 0; i < 6; i++)
	{
		Context.SetRenderTarget(mIndirectDiffuseCube->RTV(i));
		Context.SetDynamicConstantBufferView(1, sizeof(mIndirectDiffuseCube->GetCubeCameraCBV(i)), &mIndirectDiffuseCube->GetCubeCameraCBV(i));
		m_pSky->Draw(Context, &mIndirectDiffusePSO);
	}
	Context.PIXEndEvent();
}

void IBL::BakingEnvironmentSpecular(GraphicsContext& Context)
{
	assert(m_pSky);
	assert(m_pRootSignature);
	Context.PIXBeginEvent(L"Pre_Filtered Environment Map");
	Context.SetRootSignature(*m_pRootSignature);
	Context.SetViewport(mPreFilteredEnvCube->Viewport());
	Context.SetScissor(mPreFilteredEnvCube->ScissorRect());
	Context.TransitionResource(mPreFilteredEnvCube->Resource(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	for (unsigned int mip = 0; mip < MaxMipLevels; ++mip)
	{
		float Roughness = (float)mip / (float)(MaxMipLevels - 1);
		for (unsigned int i = 0; i < 6; ++i)
		{
			UINT RTVIndex = i * MaxMipLevels + mip;
			Context.SetRenderTarget(mPreFilteredEnvCube->RTV(RTVIndex));
			auto ConstantBuffer = mPreFilteredEnvCube->GetCubeCameraCBV(i);
			ConstantBuffer.mRoughness = Roughness;
			Context.SetDynamicConstantBufferView(1, sizeof(ConstantBuffer), &ConstantBuffer);
			m_pSky->Draw(Context, &mPreFilteredEnvMapPSO);
		}
	}
	Context.PIXEndEvent();
}

void IBL::BuildPSOs(const PSOCommonProperty CommonProperty)
{
	m_pRootSignature = CommonProperty.pRootSignature;

	//Baking Environment Diffuse PSO
	mIndirectDiffusePSO.SetRootSignature(*CommonProperty.pRootSignature);
	mIndirectDiffusePSO.SetRasterizerState(RasterizerDefault);
	mIndirectDiffusePSO.SetBlendState(BlendDisable);
	mIndirectDiffusePSO.SetDepthStencilState(DepthStateDisabled);
	mIndirectDiffusePSO.SetSampleMask(0xFFFFFFFF);
	mIndirectDiffusePSO.SetInputLayout((UINT)InputLayout::Pos.size(), InputLayout::Pos.data());
	mIndirectDiffusePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	mIndirectDiffusePSO.SetVertexShader(g_pBakingEnvDiffuseVS, sizeof(g_pBakingEnvDiffuseVS));
	mIndirectDiffusePSO.SetPixelShader(g_pBakingEnvDiffusePS, sizeof(g_pBakingEnvDiffusePS));
	mIndirectDiffusePSO.SetRenderTargetFormat(DefaultHDRColorFormat, DXGI_FORMAT_D32_FLOAT);
	mIndirectDiffusePSO.Finalize();

	//Pre_Filtered Environment Map PSO
	mPreFilteredEnvMapPSO = mIndirectDiffusePSO;
	mPreFilteredEnvMapPSO.SetPixelShader(g_pPreFilteredEnvironmentMapPS, sizeof(g_pPreFilteredEnvironmentMapPS));
	mPreFilteredEnvMapPSO.Finalize();

}

void IBL::PreBakeGIData(GraphicsContext& Context, RenderObject* Object)
{
	m_pSky = Object;
	BakingEnvironmentDiffuse(Context);
	BakingEnvironmentSpecular(Context);
}




