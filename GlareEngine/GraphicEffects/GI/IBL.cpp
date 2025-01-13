#include "GI/IBL.h"
#include "Sky/Sky.h"
#include "Engine/EngineLog.h"
#include "Graphics/Render.h"

//shader
#include "CompiledShaders/BakingEnvDiffusePS.h"
#include "CompiledShaders/BakingEnvDiffuseVS.h"
#include "CompiledShaders/PreFilteredEnvironmentMapPS.h"
#include "CompiledShaders/ScreenQuadVS.h"
#include "CompiledShaders/BakingBRDFPS.h"

//1024 
#define  MaxMipLevels 11

RootSignature* IBL::m_pRootSignature = nullptr;


IBL::IBL()
{
	InitMaterial();
}

IBL::~IBL()
{
}


void IBL::Initialize()
{
	mIndirectDiffuseCube = make_unique<CubeRenderTarget>(BAKECUBESIZE, BAKECUBESIZE);
	mPreFilteredEnvCube = make_unique<CubeRenderTarget>(BAKECUBESIZE, BAKECUBESIZE, MaxMipLevels);
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
	Context.SetDescriptorTable((int)RootSignatureType::eCubeTextures, Render::gTextureHeap[COMMONSRVSIZE+COMMONUAVSIZE]);
	for (int i = 0; i < 6; i++)
	{
		Context.SetRenderTarget(mIndirectDiffuseCube->RTV(i));
		CubeMapConstants constants = mIndirectDiffuseCube->GetCubeCameraCBV(i);
		Context.SetDynamicConstantBufferView((int)RootSignatureType::eCommonConstantBuffer, sizeof(mIndirectDiffuseCube->GetCubeCameraCBV(i)), &constants);
		m_pSky->Draw(Context, &mIndirectDiffuseMaterial->GetGraphicsPSO());
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
		float Roughness = (float)mip / (float)(MaxMipLevels);
		//reset viewport size for each Mipmap
		Context.SetViewport(mPreFilteredEnvCube->Viewport(mip));
		for (unsigned int i = 0; i < 6; ++i)
		{
			UINT RTVIndex = i * MaxMipLevels + mip;
			Context.SetRenderTarget(mPreFilteredEnvCube->RTV(RTVIndex));
			auto ConstantBuffer = mPreFilteredEnvCube->GetCubeCameraCBV(i);
			ConstantBuffer.mRoughness = Roughness;
			Context.SetDynamicConstantBufferView((int)RootSignatureType::eCommonConstantBuffer, sizeof(ConstantBuffer), &ConstantBuffer);
			m_pSky->Draw(Context, &mPreFilteredEnvMapMaterial->GetGraphicsPSO());
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
	Context.SetPipelineState(mBRDFMaterial->GetRuntimePSO());
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
		srvDesc.Format = IndirectDiffuseResource->GetDesc().Format;
		g_Device->CreateShaderResourceView(IndirectDiffuseResource, &srvDesc, IndirectDiffuseCubeSrv);
		GlobleSRVIndex::gBakingDiffuseCubeIndex = AddToGlobalCubeSRVDescriptor(IndirectDiffuseCubeSrv);

		//PreFilteredEnvironment
		
		D3D12_CPU_DESCRIPTOR_HANDLE PreFilteredCubeSrv = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		ID3D12Resource* PreFilteredResource = TextureManager::GetInstance(Context.GetCommandList())->GetTexture(L"PreFilteredEnvironment", false)->Resource.Get();
		srvDesc.TextureCube.MipLevels = PreFilteredResource->GetDesc().MipLevels;
		srvDesc.Format = PreFilteredResource->GetDesc().Format;
		g_Device->CreateShaderResourceView(PreFilteredResource, &srvDesc, PreFilteredCubeSrv);
		GlobleSRVIndex::gBakingPreFilteredEnvIndex = AddToGlobalCubeSRVDescriptor(PreFilteredCubeSrv);

		//Integration BRDF
		D3D12_CPU_DESCRIPTOR_HANDLE IntegrationBRDFSrv = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		ID3D12Resource* IntegrationBRDFResource = TextureManager::GetInstance(Context.GetCommandList())->GetTexture(L"IntegrationBRDF", false)->Resource.Get();
		srvDesc.Format = IntegrationBRDFResource->GetDesc().Format;
		g_Device->CreateShaderResourceView(IntegrationBRDFResource, nullptr, IntegrationBRDFSrv);
		GlobleSRVIndex::gBakingIntegrationBRDFIndex = AddToGlobalTextureSRVDescriptor(IntegrationBRDFSrv);
	}
}

void IBL::InitMaterial()
{
	mIndirectDiffuseMaterial = RenderMaterialManager::GetInstance().GetMaterial("IBL Indirect Diffuse");
	mPreFilteredEnvMapMaterial = RenderMaterialManager::GetInstance().GetMaterial("IBL PreFiltered EnvMap");
	mBRDFMaterial = RenderMaterialManager::GetInstance().GetMaterial("IBL BRDF");

	if (!mIndirectDiffuseMaterial->IsInitialized)
	{
		mIndirectDiffuseMaterial->BindPSOCreateFunc([&](const PSOCommonProperty CommonProperty) {
			m_pRootSignature = CommonProperty.pRootSignature;
			GraphicsPSO& IndirectDiffusePSO = mIndirectDiffuseMaterial->GetGraphicsPSO();

			//Baking Environment Diffuse PSO
			IndirectDiffusePSO.SetRootSignature(*CommonProperty.pRootSignature);
			IndirectDiffusePSO.SetRasterizerState(RasterizerDefault);
			IndirectDiffusePSO.SetBlendState(BlendDisable);
			IndirectDiffusePSO.SetDepthStencilState(DepthStateDisabled);
			IndirectDiffusePSO.SetSampleMask(0xFFFFFFFF);
			IndirectDiffusePSO.SetInputLayout((UINT)InputLayout::Pos.size(), InputLayout::Pos.data());
			IndirectDiffusePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			IndirectDiffusePSO.SetVertexShader(g_pBakingEnvDiffuseVS, sizeof(g_pBakingEnvDiffuseVS));
			IndirectDiffusePSO.SetPixelShader(g_pBakingEnvDiffusePS, sizeof(g_pBakingEnvDiffusePS));
			IndirectDiffusePSO.SetRenderTargetFormat(DefaultHDRColorFormat, DXGI_FORMAT_D32_FLOAT);
			IndirectDiffusePSO.Finalize();
			});

		mIndirectDiffuseMaterial->BindPSORuntimeModifyFunc([&]() {
			RuntimePSOManager::Get().RegisterPSO(&mIndirectDiffuseMaterial->GetGraphicsPSO(), GET_SHADER_PATH("GI/BakingEnvDiffuseVS.hlsl"), D3D12_SHVER_VERTEX_SHADER);
			RuntimePSOManager::Get().RegisterPSO(&mIndirectDiffuseMaterial->GetGraphicsPSO(), GET_SHADER_PATH("GI/BakingEnvDiffusePS.hlsl"), D3D12_SHVER_PIXEL_SHADER); });

		mIndirectDiffuseMaterial->IsInitialized = true;
	}

	if (!mPreFilteredEnvMapMaterial->IsInitialized)
	{
		mPreFilteredEnvMapMaterial->BindPSOCreateFunc([&](const PSOCommonProperty CommonProperty) {
			GraphicsPSO& PreFilteredEnvMapPSO = mPreFilteredEnvMapMaterial->GetGraphicsPSO();

			//Pre_Filtered Environment Map PSO
			PreFilteredEnvMapPSO.SetRootSignature(*CommonProperty.pRootSignature);
			PreFilteredEnvMapPSO.SetRasterizerState(RasterizerDefault);
			PreFilteredEnvMapPSO.SetBlendState(BlendDisable);
			PreFilteredEnvMapPSO.SetDepthStencilState(DepthStateDisabled);
			PreFilteredEnvMapPSO.SetSampleMask(0xFFFFFFFF);
			PreFilteredEnvMapPSO.SetInputLayout((UINT)InputLayout::Pos.size(), InputLayout::Pos.data());
			PreFilteredEnvMapPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			PreFilteredEnvMapPSO.SetVertexShader(g_pBakingEnvDiffuseVS, sizeof(g_pBakingEnvDiffuseVS));
			PreFilteredEnvMapPSO.SetPixelShader(g_pPreFilteredEnvironmentMapPS, sizeof(g_pPreFilteredEnvironmentMapPS));
			PreFilteredEnvMapPSO.SetRenderTargetFormat(DefaultHDRColorFormat, DXGI_FORMAT_D32_FLOAT);
			PreFilteredEnvMapPSO.Finalize();
			});

		mPreFilteredEnvMapMaterial->BindPSORuntimeModifyFunc([&]() {
			RuntimePSOManager::Get().RegisterPSO(&mPreFilteredEnvMapMaterial->GetGraphicsPSO(), GET_SHADER_PATH("GI/BakingEnvDiffuseVS.hlsl"), D3D12_SHVER_VERTEX_SHADER);
			RuntimePSOManager::Get().RegisterPSO(&mPreFilteredEnvMapMaterial->GetGraphicsPSO(), GET_SHADER_PATH("GI/PreFilteredEnvironmentMapPS.hlsl"), D3D12_SHVER_PIXEL_SHADER); });

		mPreFilteredEnvMapMaterial->IsInitialized = true;
	}

	if (!mBRDFMaterial->IsInitialized)
	{
		mBRDFMaterial->BindPSOCreateFunc([&](const PSOCommonProperty CommonProperty) {
			GraphicsPSO& BRDFPSO = mBRDFMaterial->GetGraphicsPSO();

			//Integration BRDF
			BRDFPSO.SetRootSignature(*CommonProperty.pRootSignature);
			BRDFPSO.SetRasterizerState(RasterizerDefault);
			BRDFPSO.SetBlendState(BlendDisable);
			BRDFPSO.SetDepthStencilState(DepthStateDisabled);
			BRDFPSO.SetSampleMask(0xFFFFFFFF);
			BRDFPSO.SetInputLayout((UINT)InputLayout::Pos.size(), InputLayout::Pos.data());
			BRDFPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			BRDFPSO.SetVertexShader(g_pScreenQuadVS, sizeof(g_pScreenQuadVS));
			BRDFPSO.SetPixelShader(g_pBakingBRDFPS, sizeof(g_pBakingBRDFPS));
			BRDFPSO.SetRenderTargetFormat(DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_D32_FLOAT);
			BRDFPSO.Finalize();
			});

		mBRDFMaterial->BindPSORuntimeModifyFunc([&]() {
			RuntimePSOManager::Get().RegisterPSO(&mBRDFMaterial->GetGraphicsPSO(), GET_SHADER_PATH("Misc/ScreenQuadVS.hlsl"), D3D12_SHVER_VERTEX_SHADER);
			RuntimePSOManager::Get().RegisterPSO(&mBRDFMaterial->GetGraphicsPSO(), GET_SHADER_PATH("GI/BakingBRDFPS.hlsl"), D3D12_SHVER_PIXEL_SHADER); });

		mBRDFMaterial->IsInitialized = true;
	}
}
