#include "GI/IBL.h"
#include "Sky/Sky.h"

//shader
#include "CompiledShaders/BakingEnvDiffusePS.h"
#include "CompiledShaders/BakingEnvDiffuseVS.h"
#include "CompiledShaders/PreFilteredEnvironmentMapPS.h"
#include "CompiledShaders/ScreenQuadVS.h"
#include "CompiledShaders/BakingBRDFPS.h"
#include "Engine/EngineLog.h"
#include "Graphics/Render.h"

#define  MaxMipLevels 5

GraphicsPSO IBL::mIndirectDiffusePSO;
GraphicsPSO IBL::mPreFilteredEnvMapPSO;
GraphicsPSO IBL::mBRDFPSO;

RootSignature* IBL::m_pRootSignature = nullptr;


IBL::IBL()
{
}

IBL::~IBL()
{
	mBRDFLUT->Release();
}


void IBL::Initialize()
{
	mIndirectDiffuseCube = make_unique<CubeRenderTarget>(BAKECUBESIZE, BAKECUBESIZE);
	mPreFilteredEnvCube = make_unique<CubeRenderTarget>(BAKECUBESIZE / 2, BAKECUBESIZE / 2, MaxMipLevels);
	mBRDFLUT.Create(L"Integration BRDF LUT", BAKECUBESIZE / 2, BAKECUBESIZE / 2, 1, DXGI_FORMAT_R16G16_FLOAT);
	
	GlobleSRVIndex::gBakingIntegrationBRDFIndex= AddToGlobalTextureSRVDescriptor(mBRDFLUT.GetSRV());
	GlobleSRVIndex::gBakingPreFilteredEnvIndex = mPreFilteredEnvCube->GetSRVIndex();
	GlobleSRVIndex::gBakingDiffuseCubeIndex = mIndirectDiffuseCube->GetSRVIndex();
}

void IBL::BakingEnvironmentDiffuse(GraphicsContext& Context)
{
	assert(m_pSky);
	assert(m_pRootSignature);
	Context.PIXBeginEvent(L"Baking Environment Diffuse");
	Context.SetRootSignature(*m_pRootSignature);
	//just need top mipmap size viewport
	Context.SetViewport(mIndirectDiffuseCube->Viewport(0));
	Context.SetScissor(mIndirectDiffuseCube->ScissorRect());
	Context.TransitionResource(mIndirectDiffuseCube->Resource(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	Context.SetDescriptorTable((int)RootSignatureType::eCubeTextures, Render::gTextureHeap[0]);
	for (int i = 0; i < 6; i++)
	{
		Context.SetRenderTarget(mIndirectDiffuseCube->RTV(i));
		Context.SetDynamicConstantBufferView((int)RootSignatureType::eCommonConstantBuffer, sizeof(mIndirectDiffuseCube->GetCubeCameraCBV(i)), &mIndirectDiffuseCube->GetCubeCameraCBV(i));
		m_pSky->Draw(Context, &mIndirectDiffusePSO);
	}
	Context.TransitionResource(mIndirectDiffuseCube->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	Context.PIXEndEvent();
}

void IBL::BakingPreFilteredEnvironment(GraphicsContext& Context)
{
	assert(m_pSky);
	assert(m_pRootSignature);
	Context.PIXBeginEvent(L"Pre_Filtered Environment Map");
	Context.SetRootSignature(*m_pRootSignature);
	Context.SetScissor(mPreFilteredEnvCube->ScissorRect());
	Context.TransitionResource(mPreFilteredEnvCube->Resource(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	for (unsigned int mip = 0; mip < MaxMipLevels; ++mip)
	{
		float Roughness = (float)mip / (float)(MaxMipLevels - 1);
		//reset viewport size for each Mipmap
		Context.SetViewport(mPreFilteredEnvCube->Viewport(mip));
		for (unsigned int i = 0; i < 6; ++i)
		{
			UINT RTVIndex = i * MaxMipLevels + mip;
			Context.SetRenderTarget(mPreFilteredEnvCube->RTV(RTVIndex));
			auto ConstantBuffer = mPreFilteredEnvCube->GetCubeCameraCBV(i);
			ConstantBuffer.mRoughness = Roughness;
			Context.SetDynamicConstantBufferView((int)RootSignatureType::eCommonConstantBuffer, sizeof(ConstantBuffer), &ConstantBuffer);
			m_pSky->Draw(Context, &mPreFilteredEnvMapPSO);
		}
	}
	Context.TransitionResource(mPreFilteredEnvCube->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	Context.PIXEndEvent();
}

void IBL::BakingBRDF(GraphicsContext& Context)
{
	assert(m_pRootSignature);
	Context.PIXBeginEvent(L"Integration BRDF");
	Context.SetRootSignature(*m_pRootSignature);
	Context.SetPipelineState(mBRDFPSO);
	Context.TransitionResource(mBRDFLUT, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	Context.SetRenderTarget(mBRDFLUT.GetRTV());
	Context.SetViewportAndScissor(0, 0, BAKECUBESIZE / 2, BAKECUBESIZE / 2);
	Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context.Draw(3);
	Context.TransitionResource(mBRDFLUT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	Context.PIXEndEvent();
}

void IBL::SaveBakingDataToFiles(GraphicsContext& Context)
{
	Context.Flush(true);
	mIndirectDiffuseCube->Resource().ExportToFile(TextureManager::GetTextureRootFilePath() + L"IndirectDiffuse.dds", true);
	mBRDFLUT.ExportToFile(TextureManager::GetTextureRootFilePath() + L"IntegrationBRDF.dds");
	mPreFilteredEnvCube->Resource().ExportToFile(TextureManager::GetTextureRootFilePath() + L"PreFilteredEnvironment.dds", true);
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

	//Integration BRDF
	mBRDFPSO = mIndirectDiffusePSO;
	mBRDFPSO.SetVertexShader(g_pScreenQuadVS, sizeof(g_pScreenQuadVS));
	mBRDFPSO.SetPixelShader(g_pBakingBRDFPS, sizeof(g_pBakingBRDFPS));
	mBRDFPSO.SetRenderTargetFormat(DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_D32_FLOAT);
	mBRDFPSO.Finalize();

}

void IBL::PreBakeGIData(GraphicsContext& Context, RenderObject* Object)
{
	m_pSky = Object;

	Initialize();

	bool isInitialized = CheckFileExist(TextureManager::GetTextureRootFilePath() + L"IndirectDiffuse.dds")&&
		CheckFileExist(TextureManager::GetTextureRootFilePath() + L"PreFilteredEnvironment.dds")&&
		CheckFileExist(TextureManager::GetTextureRootFilePath() + L"IntegrationBRDF.dds");

	if (!isInitialized)
	{
		EngineLog::AddLog(L"PreBaking GI Data!");
		Render::CopyTextureHeap();
		Context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, Render::gTextureHeap.GetHeapPointer());
		BakingEnvironmentDiffuse(Context);
		BakingPreFilteredEnvironment(Context);
		BakingBRDF(Context);
		SaveBakingDataToFiles(Context);
	}
	else
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = 1;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

		// EnvironmentDiffuse
		D3D12_CPU_DESCRIPTOR_HANDLE IndirectDiffuseCubeSrv = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		ID3D12Resource* IndirectDiffuseResource = TextureManager::GetInstance(Context.GetCommandList())->GetTexture(L"IndirectDiffuse", false)->Resource.Get();
		g_Device->CreateShaderResourceView(IndirectDiffuseResource, &srvDesc, IndirectDiffuseCubeSrv);
		GlobleSRVIndex::gBakingDiffuseCubeIndex = AddToGlobalCubeSRVDescriptor(IndirectDiffuseCubeSrv);

		//PreFilteredEnvironment
		srvDesc.TextureCube.MipLevels = MaxMipLevels;
		D3D12_CPU_DESCRIPTOR_HANDLE PreFilteredCubeSrv = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		ID3D12Resource* PreFilteredResource = TextureManager::GetInstance(Context.GetCommandList())->GetTexture(L"PreFilteredEnvironment", false)->Resource.Get();
		g_Device->CreateShaderResourceView(PreFilteredResource, &srvDesc, PreFilteredCubeSrv);
		GlobleSRVIndex::gBakingPreFilteredEnvIndex = AddToGlobalCubeSRVDescriptor(PreFilteredCubeSrv);

		//Integration BRDF
		D3D12_CPU_DESCRIPTOR_HANDLE IntegrationBRDFSrv = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		ID3D12Resource* IntegrationBRDFResource = TextureManager::GetInstance(Context.GetCommandList())->GetTexture(L"IntegrationBRDF", false)->Resource.Get();
		g_Device->CreateShaderResourceView(IntegrationBRDFResource, nullptr, IntegrationBRDFSrv);
		GlobleSRVIndex::gBakingIntegrationBRDFIndex = AddToGlobalTextureSRVDescriptor(IntegrationBRDFSrv);
	}
}




