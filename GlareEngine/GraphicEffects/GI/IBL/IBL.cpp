#include "IBL.h"
#include "Sky.h"

//shader
#include "CompiledShaders/BakingEnvDiffusePS.h"
#include "CompiledShaders/BakingEnvDiffuseVS.h"


GraphicsPSO IBL::mIndirectDiffusePSO;
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

}

void IBL::BakingEnvironmentDiffuse(GraphicsContext& Context)
{
	assert(m_pSky);
	assert(m_pRootSignature);

	Context.SetRootSignature(*m_pRootSignature);
	Context.SetViewport(mIndirectDiffuseCube->Viewport());
	Context.SetScissor(mIndirectDiffuseCube->ScissorRect());
	Context.PIXBeginEvent(L"Baking Environment Diffuse");
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
}

void IBL::BuildPSOs(const PSOCommonProperty CommonProperty)
{
	//Baking Environment Specular PSO
	m_pRootSignature =CommonProperty.pRootSignature;
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




}

void IBL::PreBakeGIData(GraphicsContext& Context, RenderObject* Object)
{
	m_pSky = Object;
	BakingEnvironmentDiffuse(Context);
	BakingEnvironmentSpecular(Context);
}




