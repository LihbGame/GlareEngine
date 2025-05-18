#include "Scene.h"
#include "Animation/ModelLoader.h"
#include "Shadow/ShadowMap.h"
#include "EngineGUI.h"
#include "Graphics/Render.h"
#include "Model/Model.h"
#include "InstanceModel/glTFInstanceModel.h"
#include "Model/MeshRenderPass.h"
#include "Engine/EngineProfiling.h"
#include "Misc/LightManager.h"
#include "PostProcessing/PostProcessing.h"
#include "PostProcessing/SSAO.h"
#include "PostProcessing/TemporalAA.h"

/// Scene/////////////////////////////////////////////
using namespace GlareEngine::Render;


Scene::Scene(string name, ID3D12GraphicsCommandList* pCommandList)
	:mName(name),
	m_pCommandList(pCommandList)
{
	mSceneView.m_pScene = this;
}

void Scene::Update(float DeltaTime)
{
	//Clear visible buffer for debug
	EngineGUI::ClearRenderPassVisualizeTexture();

	//Update shadow map
	m_pShadowMap->Update(DeltaTime);

	//Update Main Constant Buffer
	UpdateMainConstantBuffer(DeltaTime);

	//Update Objects
	for (auto& object : m_pRenderObjects)
	{
		object->Update(DeltaTime);
	}

	GraphicsContext& Context = GraphicsContext::Begin(L"Scene Update");

	//Update GLTF Objects
	for (auto& object : m_pGLTFRenderObjects)
	{
		object->Update(DeltaTime, &Context);
	}

	//update Render Pipeline Type
	if (LoadingFinish)
	{
		RasterRenderPipelineType NewRasterRenderPipelineType = static_cast<RasterRenderPipelineType>(m_pGUI->GetRasterRenderPipelineIndex());

		if (NewRasterRenderPipelineType != Render::gRasterRenderPipelineType && !gCommonProperty.IsWireframe)
		{
			Render::gRasterRenderPipelineType = NewRasterRenderPipelineType;
			Render::BuildPSOs();
		}
	}

	if (gCommonProperty.IsWireframe)
	{
		if (Render::gRasterRenderPipelineType != RasterRenderPipelineType::TBFR)
		{
			Render::gRasterRenderPipelineType = RasterRenderPipelineType::TBFR;
			Render::BuildPSOs();
		}
	}

	//Update Screen Processing
	ScreenProcessing::Update(DeltaTime, mSceneView.mMainConstants, *mSceneView.m_pCamera);

	//lighting update
	Lighting::Update(mSceneView.mMainConstants, *mSceneView.m_pCamera);

	Context.Finish();

}

void Scene::VisibleUpdateForType()
{
	unordered_map<ObjectType, bool> TypeVisible;
	TypeVisible[ObjectType::Sky]		= m_pGUI->IsShowSky();
	TypeVisible[ObjectType::Model]		= m_pGUI->IsShowModel();
	TypeVisible[ObjectType::Shadow]		= m_pGUI->IsShowShadow();

	bool ShadowVisible = TypeVisible[ObjectType::Shadow];

	for (auto& object : m_pRenderObjects)
	{
		//update Model shadow visible
		if (object->mObjectType == ObjectType::Model)
		{
			object->SetShadowRenderFlag(ShadowVisible && object->GetShadowFlag());
		}

		if (TypeVisible[object->mObjectType] != object->GetVisible())  //update visible
		{
			object->SetVisible(TypeVisible[object->mObjectType]);
		}
	}

	for (auto& object : m_pGLTFRenderObjects)
	{
		//update Model shadow visible
		if (object->mObjectType == ObjectType::Model)
		{
			object->SetShadowRenderFlag(ShadowVisible && object->GetShadowFlag());
		}

		if (TypeVisible[object->mObjectType] != object->GetVisible())  //update visible
		{
			object->SetVisible(TypeVisible[object->mObjectType]);
		}
	}
}

void Scene::BakingGIData(GraphicsContext& Context)
{
	mIBLGI.PreBakeGIData(Context, m_pRenderObjectsType[(int)ObjectType::Sky].front());
}

void Scene::SetSceneLights(DirectionalLight* light, int DirectionalLightsCount)
{
	mSceneView.mMainConstants.gDirectionalLightsCount = DirectionalLightsCount;
	int CopySize = DirectionalLightsCount * sizeof(DirectionalLight);
	memcpy_s(mSceneLights, CopySize, light, CopySize);

	if (mName == "Sponza")
	{
		mSceneView.mMainConstants.gIsIndoorScene = true;
		mSceneLights[0].Strength = { 20.0f, 20.0f, 20.0f };
	}
	else if (mName == "Blue Tree")
	{
		mSceneLights[0].Strength = { 5.0f,  5.0f,  5.0f };
	}
}

void Scene::SetRootSignature(RootSignature* rootSignature)
{
	m_pRootSignature = rootSignature;
}

void Scene::Finalize()
{
	LoadingFinish = true;

	mPRTManager.Initialize(mSceneAABB, PROBE_CELL_SIZE);
}


void Scene::AddObjectToScene(RenderObject* Object)
{
	switch (Object->mObjectType)
	{
	case ObjectType::Sky: {
		m_pRenderObjectsType[(int)ObjectType::Sky].push_back(Object);
		m_pRenderObjects.push_back(Object);
		break;
	}
	case ObjectType::Model:{
		m_pRenderObjects.push_back(Object);
		break;
	}
	default:{
		m_pRenderObjects.push_back(Object);
		break;
	}
	}
}

void Scene::AddGLTFModelToScene(RenderObject* Object)
{
	ModelInstance* model = dynamic_cast<glTFInstanceModel*>(Object)->GetModel();
	model->LoopAllAnimations();
	m_pGLTFRenderObjects.push_back(Object);
	mSceneAABB.AddBoundingBox(model->GetAxisAlignedBox());
}

void Scene::RenderScene(RasterRenderPipelineType Type)
{
	if (LoadingFinish)
	{
		//mPRTManager.GenerateSHProbes(*this);
		//Update(Display::GetFrameTime());

	}

	switch (Type)
	{
	case RasterRenderPipelineType::FR:
	{
		ForwardRendering(RasterRenderPipelineType::FR);
		break;
	}
	case RasterRenderPipelineType::TBFR:
	{
		ForwardRendering(RasterRenderPipelineType::TBFR);
		break;
	}
	case RasterRenderPipelineType::CBFR:
	{
		ForwardRendering(RasterRenderPipelineType::CBFR);
		break;
	}
	case RasterRenderPipelineType::TBDR:
	{
		DeferredRendering(RasterRenderPipelineType::TBDR);
		break;
	}
	case RasterRenderPipelineType::CBDR:
	{
		DeferredRendering(RasterRenderPipelineType::CBDR);
		break;
	}
	default:
		break;
	}

	//Post Processing
	ScreenProcessing::Render(*mSceneView.m_pCamera);
}

void Scene::DrawUI()
{
	GraphicsContext& RenderContext = GraphicsContext::Begin(L"Render UI");
	//Draw UI
	RenderContext.SetRenderTarget(Display::GetCurrentBuffer().GetRTV());
	RenderContext.TransitionResource(Display::GetCurrentBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	m_pGUI->BeginDraw(RenderContext.GetCommandList());
	
	if (m_pGUI->GetRenderPipelineIndex()==RenderPipeline::Rasterization
		|| m_pGUI->GetRenderPipelineIndex()==RenderPipeline::Hybrid)
	{
		//Lighting UI
		Lighting::DrawUI();

		mPRTManager.DrawUI();
	
		for (auto& RenderObject : m_pRenderObjects)
		{
			if (RenderObject->GetVisible())
			{
				RenderObject->DrawUI();
			}
		}

		//Post Processing UI
		ScreenProcessing::DrawUI();
	}
	else // RenderPipeline::RayTracing
	{
		
	}
	
	m_pGUI->EndDraw(RenderContext.GetCommandList());
	RenderContext.TransitionResource(Display::GetCurrentBuffer(), D3D12_RESOURCE_STATE_PRESENT, true);
	RenderContext.Finish();
}

void Scene::ReleaseScene()
{
}

void Scene::ResizeViewport(uint32_t width, uint32_t height)
{
	mSceneView.m_MainViewport.Width = (float)width;
	mSceneView.m_MainViewport.Height = (float)height;
	mSceneView.m_MainViewport.MinDepth = 0.0f;
	mSceneView.m_MainViewport.MaxDepth = 1.0f;

	mSceneView.m_MainScissor.left = 0;
	mSceneView.m_MainScissor.top = 0;
	mSceneView.m_MainScissor.right = (LONG)width;
	mSceneView.m_MainScissor.bottom = (LONG)height;
}


void Scene::UpdateMainConstantBuffer(float DeltaTime)
{
	XMFLOAT4X4 TempFloat4x4 = mSceneView.m_pCamera->GetView4x4f();
	XMMATRIX View = XMLoadFloat4x4(&TempFloat4x4);
	TempFloat4x4 = mSceneView.m_pCamera->GetProj4x4f();
	XMMATRIX Proj = XMLoadFloat4x4(&TempFloat4x4);

	XMMATRIX ViewProj = XMMatrixMultiply(View, Proj);
	XMVECTOR Determinant = XMMatrixDeterminant(View);
	XMMATRIX InvView = XMMatrixInverse(&Determinant, View);
	Determinant = XMMatrixDeterminant(Proj);
	XMMATRIX InvProj = XMMatrixInverse(&Determinant, Proj);
	Determinant = XMMatrixDeterminant(ViewProj);
	XMMATRIX InvViewProj = XMMatrixInverse(&Determinant, ViewProj);

	XMStoreFloat4x4(&mSceneView.mMainConstants.View, XMMatrixTranspose(View));
	XMStoreFloat4x4(&mSceneView.mMainConstants.InvView, XMMatrixTranspose(InvView));
	XMStoreFloat4x4(&mSceneView.mMainConstants.Proj, XMMatrixTranspose(Proj));
	XMStoreFloat4x4(&mSceneView.mMainConstants.InvProj, XMMatrixTranspose(InvProj));
	XMStoreFloat4x4(&mSceneView.mMainConstants.ViewProj, XMMatrixTranspose(ViewProj));
	XMStoreFloat4x4(&mSceneView.mMainConstants.InvViewProj, XMMatrixTranspose(InvViewProj));
	XMFLOAT4X4 ShadowTransform = m_pShadowMap->GetShadowTransform();
	XMStoreFloat4x4(&mSceneView.mMainConstants.ShadowTransform, XMMatrixTranspose(XMLoadFloat4x4(&ShadowTransform)));
	
	mSceneView.mMainConstants.EyePosW = mSceneView.m_pCamera->GetPosition3f();
	mSceneView.mMainConstants.RenderTargetSize = XMFLOAT2((float)g_SceneColorBuffer.GetWidth(), (float)g_SceneColorBuffer.GetHeight());
	mSceneView.mMainConstants.InvRenderTargetSize = XMFLOAT2(1.0f / g_SceneColorBuffer.GetWidth(), 1.0f / g_SceneColorBuffer.GetHeight());
	mSceneView.mMainConstants.NearZ = mSceneView.m_pCamera->GetNearZ();
	mSceneView.mMainConstants.FarZ = mSceneView.m_pCamera->GetFarZ();
	mSceneView.mMainConstants.TotalTime = GameTimer::TotalTime();
	mSceneView.mMainConstants.DeltaTime = DeltaTime;
	mSceneView.mMainConstants.gIsClusterBaseLighting = (gRasterRenderPipelineType == RasterRenderPipelineType::CBDR || gRasterRenderPipelineType == RasterRenderPipelineType::CBFR) ? 1 : 0;

	mSceneView.mMainConstants.gTemporalJitter = TemporalAA::GetJitterOffset();
	mSceneView.mMainConstants.ZMagic= (mSceneView.mMainConstants.FarZ - mSceneView.mMainConstants.NearZ) / mSceneView.mMainConstants.NearZ;

	//Tiled light data
	mSceneView.mMainConstants.InvTileDim[0] = 1.0f / Lighting::LightGridDimension;
	mSceneView.mMainConstants.InvTileDim[1] = 1.0f / Lighting::LightGridDimension;
	mSceneView.mMainConstants.TileCount[0] = Math::DivideByMultiple(g_SceneColorBuffer.GetWidth(), Lighting::LightGridDimension);
	mSceneView.mMainConstants.TileCount[1] = Math::DivideByMultiple(g_SceneColorBuffer.GetHeight(), Lighting::LightGridDimension);

	mSceneView.mMainConstants.bEnableAreaLight=Lighting::bEnableAreaLight;
	mSceneView.mMainConstants.bEnableConeLight=Lighting::bEnableConeLight;
	mSceneView.mMainConstants.bEnablePointLight=Lighting::bEnablePointLight;
	mSceneView.mMainConstants.bEnableDirectionalLight=Lighting::bEnableDirectionalLight;

	mSceneView.mMainConstants.gAreaLightLTC1SRVIndex=Lighting::AreaLightLTC1SRVIndex;
	mSceneView.mMainConstants.gAreaLightLTC2SRVIndex=Lighting::AreaLightLTC2SRVIndex;
	
	//Shadow Map 
	mSceneView.mMainConstants.mShadowMapIndex = m_pShadowMap->GetShadowMapIndex();

	assert(GlobleSRVIndex::gSkyCubeSRVIndex >= 0);
	assert(GlobleSRVIndex::gBakingDiffuseCubeIndex >= 0);
	assert(GlobleSRVIndex::gBakingPreFilteredEnvIndex >= 0);
	assert(GlobleSRVIndex::gBakingIntegrationBRDFIndex >= 0);
	//GI DATA
	mSceneView.mMainConstants.mSkyCubeIndex = GlobleSRVIndex::gSkyCubeSRVIndex;
	mSceneView.mMainConstants.mBakingDiffuseCubeIndex = GlobleSRVIndex::gBakingDiffuseCubeIndex;
	mSceneView.mMainConstants.gBakingPreFilteredEnvIndex = GlobleSRVIndex::gBakingPreFilteredEnvIndex;
	mSceneView.mMainConstants.gBakingIntegrationBRDFIndex = GlobleSRVIndex::gBakingIntegrationBRDFIndex;
	mSceneView.mMainConstants.IsRenderTiledBaseLighting = Lighting::bEnableTiledBaseLight;
	mSceneView.mMainConstants.gAreaLightTwoSide = Lighting::bEnableAreaLightTwoSide;
	//Lights constant buffer
	{
		mSceneView.mMainConstants.gAmbientLight = { 0.05f, 0.05f, 0.05f, 1.0f };

		mSceneView.mMainConstants.Lights[0].Direction = mSceneLights[0].Direction;
		mSceneView.mMainConstants.Lights[0].Strength = mSceneLights[0].Strength;

		mSceneView.mMainConstants.Lights[1].Direction = mSceneLights[1].Direction;
		mSceneView.mMainConstants.Lights[1].Strength = mSceneLights[1].Strength;

		mSceneView.mMainConstants.Lights[2].Direction = mSceneLights[2].Direction;
		mSceneView.mMainConstants.Lights[2].Strength = mSceneLights[2].Strength;
	}

}


void Scene::CreateShadowMap(GraphicsContext& Context,vector<RenderObject*> RenderObjects)
{
	assert(m_pShadowMap);
	m_pShadowMap->Draw(Context,RenderObjects);
}

void Scene::CreateShadowMapForGLTF(GraphicsContext& Context)
{
	ScopedTimer SunShadowScope(L"Shadow Map", Context);

	MeshRenderPass ShadowMeshPass(MeshRenderPass::eShadows);
	ShadowMeshPass.SetCamera(*m_pSunShadowCamera.get());
	ShadowMeshPass.SetDepthStencilTarget(m_pSunShadowCamera->GetShadowMap()->GetShadowBuffer());

	for (auto& model : m_pGLTFRenderObjects)
	{
		if (model->GetVisible() && model->GetShadowRenderFlag())
		{
			dynamic_cast<glTFInstanceModel*>(model)->GetModel()->AddToRender(ShadowMeshPass);
		}
	}
	ShadowMeshPass.Sort();
	ShadowMeshPass.RenderMeshes(MeshRenderPass::eZPass, Context, mSceneView.mMainConstants);
}

void Scene::ForwardRendering()
{
	GraphicsContext& Context = GraphicsContext::Begin(L"RenderScene");
#pragma region Scene
	Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

	Context.ClearDepthAndStencil(g_SceneDepthBuffer, REVERSE_Z ? 0.0f : 1.0f);

	Context.ClearRenderTarget(g_SceneColorBuffer);

	Context.SetRootSignature(*m_pRootSignature);

	Context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, gTextureHeap.GetHeapPointer());

	//set main constant buffer
	Context.SetDynamicConstantBufferView((int)RootSignatureType::eMainConstantBuffer, sizeof(mSceneView.mMainConstants), &mSceneView.mMainConstants);
	//Set Cube SRV
	Context.SetDescriptorTable((int)RootSignatureType::eCubeTextures, gTextureHeap[COMMONSRVSIZE+ COMMONUAVSIZE]);
	//Set Textures SRV
	Context.SetDescriptorTable((int)RootSignatureType::ePBRTextures, gTextureHeap[MAXCUBESRVSIZE+ COMMONSRVSIZE+ COMMONUAVSIZE]);
	//Set Material Data
	const vector<MaterialConstant>& MaterialData = MaterialManager::GetMaterialInstance()->GetMaterialsConstantBuffer();
	Context.SetDynamicSRV((int)RootSignatureType::eMaterialConstantData, sizeof(MaterialConstant) * MaterialData.size(), MaterialData.data());

	Context.PIXBeginEvent(L"Shadow Pass");
	CreateShadowMap(Context,m_pRenderObjects);
	Context.PIXEndEvent();

	//set Viewport And Scissor
	Context.SetViewportAndScissor(mSceneView.m_MainViewport, mSceneView.m_MainScissor);
	


	//MSAA
	if (IsMSAA)
	{
		Context.TransitionResource(g_SceneMSAAColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		Context.TransitionResource(g_SceneMSAADepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
		Context.ClearDepth(g_SceneMSAADepthBuffer, REVERSE_Z ? 0.0f : 1.0f);
		Context.ClearRenderTarget(g_SceneMSAAColorBuffer);
		//set scene render target & Depth Stencil target
		Context.SetRenderTarget(g_SceneMSAAColorBuffer.GetRTV(), g_SceneMSAADepthBuffer.GetDSV());
	}
	else
	{
		//set scene render target & Depth Stencil target
		Context.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV());
	}

	Context.PIXBeginEvent(L"Main Pass");
	for (auto& RenderObject : m_pRenderObjects)
	{
		if (RenderObject->GetVisible())
		{
			Context.PIXBeginEvent(RenderObject->GetName().c_str());
			RenderObject->Draw(Context);
			Context.PIXEndEvent();
		}
	}
	
	Context.PIXEndEvent();

	//MSAA
	if (IsMSAA)
	{
		Context.TransitionResource(g_SceneMSAAColorBuffer, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
		Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RESOLVE_DEST, true);
		Context.TransitionResource(g_SceneMSAADepthBuffer, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
		Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_RESOLVE_DEST, true);
		Context.GetCommandList()->ResolveSubresource(g_SceneColorBuffer.GetResource(), 0, g_SceneMSAAColorBuffer.GetResource(), 0, DefaultHDRColorFormat);
		Context.GetCommandList()->ResolveSubresource(g_SceneDepthBuffer.GetResource(), 0, g_SceneMSAADepthBuffer.GetResource(), 0, DXGI_FORMAT_R32_FLOAT);
	}

	//Linear Z
	{
		ScopedTimer LinearZScope(L"Linear Z", Context);

		ComputeContext& computeContext = Context.GetComputeContext();

		uint32_t frameIndex = 0; //change it when add TAA

		ScreenProcessing::LinearizeZ(computeContext, *mSceneView.m_pCamera, frameIndex);
	}

	//SSAO
	{
		SSAO::Render(Context, mSceneView.mMainConstants);
	}

#pragma endregion
	Context.Finish();
}

void Scene::ForwardRendering(RasterRenderPipelineType ForwardRenderPipeline)
{
	if (LoadingFinish)
	{
		//Flush pso after loading
		static bool flushPSO = true;
		if (flushPSO)
		{
			Render::BuildPSOs();
			Render::BuildRuntimePSOs();
			flushPSO = false;
		}

		Render::SetCommonSRVHeapOffset(GBUFFER_TEXTURE_REGISTER_COUNT);

		if (ForwardRenderPipeline == RasterRenderPipelineType::FR)
		{
			ForwardRendering();
		}
		else
		{
			GraphicsContext& Context = GraphicsContext::Begin(L"Scene Render");

			//default batch
			MeshRenderPass DefaultMeshPass(MeshRenderPass::eDefault);
			DefaultMeshPass.SetCamera(*mSceneView.m_pCamera);
			DefaultMeshPass.SetViewport(mSceneView.m_MainViewport);
			DefaultMeshPass.SetScissor(mSceneView.m_MainScissor);

			if (IsMSAA)
			{
				DefaultMeshPass.SetDepthStencilTarget(g_SceneMSAADepthBuffer);
				DefaultMeshPass.AddRenderTarget(g_SceneMSAAColorBuffer);
			}
			else
			{
				DefaultMeshPass.SetDepthStencilTarget(g_SceneDepthBuffer);
				DefaultMeshPass.AddRenderTarget(g_SceneColorBuffer);
			}

			//Culling and add  Render Objects
			for (auto& model : m_pGLTFRenderObjects)
			{
				if (model->GetVisible())
				{
					dynamic_cast<glTFInstanceModel*>(model)->GetModel()->AddToRender(DefaultMeshPass);
				}
			}

			//Sort Visible objects
			DefaultMeshPass.Sort();

			//Depth PrePass
			{
				ScopedTimer PrePassScope(L"Depth PrePass", Context);

				//Clear Depth And Stencil
				if (IsMSAA)
				{
					Context.TransitionResource(g_SceneMSAADepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
					Context.ClearDepthAndStencil(g_SceneMSAADepthBuffer, REVERSE_Z ? 0.0f : 1.0f);
				}
				else
				{
					Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
					Context.ClearDepthAndStencil(g_SceneDepthBuffer, REVERSE_Z ? 0.0f : 1.0f);
				}

				DefaultMeshPass.RenderMeshes(MeshRenderPass::eZPass, Context, mSceneView.mMainConstants);

				//Resolve Depth Buffer if MSAA
				if (IsMSAA)
				{
					Context.TransitionResource(g_SceneMSAADepthBuffer, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
					Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_RESOLVE_DEST, true);
					Context.GetCommandList()->ResolveSubresource(g_SceneDepthBuffer.GetResource(), 0, g_SceneMSAADepthBuffer.GetResource(), 0, DXGI_FORMAT_R32_FLOAT);
				}
			}

			//Linear Z
			{
				ScopedTimer LinearZScope(L"Linear Z", Context);

				ComputeContext& computeContext = Context.GetComputeContext();

				uint32_t frameIndex = TemporalAA::GetFrameIndexMod2();

				ScreenProcessing::LinearizeZ(computeContext, *mSceneView.m_pCamera, frameIndex);
			}

			//SSAO
			{
				SSAO::Render(Context, mSceneView.mMainConstants);

				CopyBufferDescriptors(&g_SSAOFullScreen.GetSRV(), 1, &gTextureHeap[Render::GetCommonSRVHeapOffset()]);
				Render::SetCommonSRVHeapOffset(Render::GetCommonSRVHeapOffset() + 1);
			}

			if (ForwardRenderPipeline == RasterRenderPipelineType::TBFR)
			{
				//Light Culling
				Lighting::FillLightGrid(Context, *mSceneView.m_pCamera);
			}
			else if (ForwardRenderPipeline == RasterRenderPipelineType::CBFR)
			{
				//Light Culling
				Lighting::BuildCluster(Context, mSceneView.mMainConstants);
				Lighting::MaskUnUsedCluster(Context, mSceneView.mMainConstants);
				Lighting::ClusterLightingCulling(Context);
			}

			//Copy PBR SRV
			D3D12_CPU_DESCRIPTOR_HANDLE PBR_SRV[] =
			{
				Lighting::m_LightGrid.GetSRV(),
				Lighting::m_LightBuffer.GetSRV(),
				Lighting::m_LightShadowArray.GetSRV(),
				Lighting::m_ClusterLightGrid.GetSRV(),
				Lighting::m_GlobalLightIndexList.GetSRV(),
				Lighting::m_GlobalRectAreaLightData.GetSRV()
			};

			CopyBufferDescriptors(PBR_SRV, ArraySize(PBR_SRV), &gTextureHeap[Render::GetCommonSRVHeapOffset()]);
			Render::SetCommonSRVHeapOffset(Render::GetCommonSRVHeapOffset() + ArraySize(PBR_SRV));

			//set descriptor heap 
			Context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, gTextureHeap.GetHeapPointer());
			//Set Common SRV
			Context.SetDescriptorTable((int)RootSignatureType::eCommonSRVs, gTextureHeap[0]);

			{
				ScopedTimer MainRenderScope(L"Main Render", Context);

				if (LoadingFinish)
				{
					CreateTileConeShadowMap(Context);
				}


				//Sun Shadow Map
				CreateShadowMapForGLTF(Context);

				//Base Pass
				{
					ScopedTimer RenderColorScope(L"Render Color", Context);

					//Clear Render Target
					if (IsMSAA)
					{
						Context.TransitionResource(g_SceneMSAAColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
						Context.ClearRenderTarget(g_SceneMSAAColorBuffer);
					}
					else
					{
						Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
						Context.ClearRenderTarget(g_SceneColorBuffer);
					}

					//Set Cube SRV
					Context.SetDescriptorTable((int)RootSignatureType::eCubeTextures, gTextureHeap[COMMONSRVSIZE + COMMONUAVSIZE]);
					//Set Textures SRV
					Context.SetDescriptorTable((int)RootSignatureType::ePBRTextures, gTextureHeap[MAXCUBESRVSIZE + COMMONSRVSIZE + COMMONUAVSIZE]);

					if (IsMSAA)
					{
						Context.TransitionResource(g_SceneMSAADepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
						Context.SetRenderTarget(g_SceneMSAAColorBuffer.GetRTV(), g_SceneMSAADepthBuffer.GetDSV_DepthReadOnly());
					}
					else
					{
						Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
						Context.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV_DepthReadOnly());
					}
					Context.SetViewportAndScissor(mSceneView.m_MainViewport, mSceneView.m_MainScissor);

					//Sky
					if (m_pRenderObjectsType[(int)ObjectType::Sky].front()->GetVisible())
					{
						Context.SetDynamicConstantBufferView((int)RootSignatureType::eMainConstantBuffer, sizeof(MainConstants), &mSceneView.mMainConstants);
						m_pRenderObjectsType[(int)ObjectType::Sky].front()->Draw(Context);
					}

					DefaultMeshPass.RenderMeshes(MeshRenderPass::eOpaque, Context, mSceneView.mMainConstants);

				}

				//Transparent Pass
				DefaultMeshPass.RenderMeshes(MeshRenderPass::eTransparent, Context, mSceneView.mMainConstants);

				//Resolve MSAA
				if (IsMSAA)
				{
					Context.TransitionResource(g_SceneMSAAColorBuffer, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
					Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RESOLVE_DEST, true);
					Context.GetCommandList()->ResolveSubresource(g_SceneColorBuffer.GetResource(), 0, g_SceneMSAAColorBuffer.GetResource(), 0, DefaultHDRColorFormat);
				}
			}

			//After Lighting Pass
			{
				mPRTManager.DebugVisual(Context);
				Lighting::RenderAreaLightMesh(Context);
			}

			Context.Finish();
		}
	}
}


void Scene::DeferredRendering(RasterRenderPipelineType DeferredRenderPipeline)
{
	if (LoadingFinish)
	{
		//Flush pso after loading
		static bool flushPSO = true;
		if (flushPSO)
		{
			Render::BuildPSOs();
			Render::BuildRuntimePSOs();
			flushPSO = false;
		}

		GraphicsContext& Context = GraphicsContext::Begin(L"Scene Render");

		Render::SetCommonSRVHeapOffset(GBUFFER_TEXTURE_REGISTER_COUNT);

		{
			ScopedTimer ClearGBufferScope(L"Clear GBuffer", Context);
			Render::ClearGBuffer(Context);
		}

		//default batch
		MeshRenderPass DefaultMeshPass(MeshRenderPass::eDefault);
		DefaultMeshPass.SetCamera(*mSceneView.m_pCamera);
		DefaultMeshPass.SetViewport(mSceneView.m_MainViewport);
		DefaultMeshPass.SetScissor(mSceneView.m_MainScissor);

		DefaultMeshPass.SetDepthStencilTarget(g_SceneDepthBuffer);
		DefaultMeshPass.AddRenderTarget(g_SceneColorBuffer);

		//Culling and add  Render Objects
		for (auto& model : m_pGLTFRenderObjects)
		{
			if (model->GetVisible())
			{
				dynamic_cast<glTFInstanceModel*>(model)->GetModel()->AddToRender(DefaultMeshPass);
			}
		}

		//Sort Visible objects
		DefaultMeshPass.Sort();

		//Depth PrePass
		{
			ScopedTimer PrePassScope(L"Depth PrePass", Context);

			//Clear Depth And Stencil
			Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
			Context.ClearDepthAndStencil(g_SceneDepthBuffer, REVERSE_Z ? 0.0f : 1.0f);

			DefaultMeshPass.RenderMeshes(MeshRenderPass::eZPass, Context, mSceneView.mMainConstants);
		}

		//Linear Z
		{
			ScopedTimer LinearZScope(L"Linear Z", Context);

			ComputeContext& computeContext = Context.GetComputeContext();

			uint32_t frameIndex = TemporalAA::GetFrameIndexMod2();

			ScreenProcessing::LinearizeZ(computeContext, *mSceneView.m_pCamera, frameIndex);
		}

		//SSAO
		{
			SSAO::Render(Context, mSceneView.mMainConstants);

			CopyBufferDescriptors(&g_SSAOFullScreen.GetSRV(), 1, &gTextureHeap[Render::GetCommonSRVHeapOffset()]);
			Render::SetCommonSRVHeapOffset(Render::GetCommonSRVHeapOffset() + 1);
		}

		if (DeferredRenderPipeline == RasterRenderPipelineType::TBDR)
		{
			//Light Culling
			Lighting::FillLightGrid(Context, *mSceneView.m_pCamera);
		}
		else if (DeferredRenderPipeline == RasterRenderPipelineType::CBDR)
		{
			ScopedTimer _prof(L"Fill Cluster Grid", Context);
			//Light Culling
			Lighting::BuildCluster(Context, mSceneView.mMainConstants);
			Lighting::MaskUnUsedCluster(Context, mSceneView.mMainConstants);
			Lighting::ClusterLightingCulling(Context);
		}

		//Copy PBR SRV
		D3D12_CPU_DESCRIPTOR_HANDLE PBR_SRV[] =
		{
			Lighting::m_LightGrid.GetSRV(),
			Lighting::m_LightBuffer.GetSRV(),
			Lighting::m_LightShadowArray.GetSRV(),
			Lighting::m_ClusterLightGrid.GetSRV(),
			Lighting::m_GlobalLightIndexList.GetSRV(),
			Lighting::m_GlobalRectAreaLightData.GetSRV()
		};

		CopyBufferDescriptors(PBR_SRV, ArraySize(PBR_SRV), &gTextureHeap[Render::GetCommonSRVHeapOffset()]);
		Render::SetCommonSRVHeapOffset(Render::GetCommonSRVHeapOffset() + ArraySize(PBR_SRV));


		//set descriptor heap 
		Context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, gTextureHeap.GetHeapPointer());
		//Set Common SRV
		Context.SetDescriptorTable((int)RootSignatureType::eCommonSRVs, gTextureHeap[0]);

		{
			ScopedTimer MainRenderScope(L"Main Render", Context);

			if (LoadingFinish)
			{
				CreateTileConeShadowMap(Context);
			}

			//Sun Shadow Map
			CreateShadowMapForGLTF(Context);

			//Base Pass
			{
				ScopedTimer BasePassScope(L"Base Pass", Context);

				//Clear Render Target
				Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
				Context.ClearRenderTarget(g_SceneColorBuffer);

				//Set Cube SRV
				Context.SetDescriptorTable((int)RootSignatureType::eCubeTextures, gTextureHeap[COMMONSRVSIZE + COMMONUAVSIZE]);
				//Set Textures SRV
				Context.SetDescriptorTable((int)RootSignatureType::ePBRTextures, gTextureHeap[MAXCUBESRVSIZE + COMMONSRVSIZE + COMMONUAVSIZE]);

				DefaultMeshPass.RenderMeshes(MeshRenderPass::eOpaque, Context, mSceneView.mMainConstants);

			}

			//Sky
			{
				ScopedTimer SkyPassScope(L"Sky", Context);
				Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
				Context.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV_DepthReadOnly());
				Context.SetViewportAndScissor(mSceneView.m_MainViewport, mSceneView.m_MainScissor);
				if (m_pRenderObjectsType[(int)ObjectType::Sky].front()->GetVisible())
				{
					Context.SetDynamicConstantBufferView((int)RootSignatureType::eMainConstantBuffer, sizeof(MainConstants), &mSceneView.mMainConstants);
					m_pRenderObjectsType[(int)ObjectType::Sky].front()->Draw(Context);
				}
			}

			//Lighting Pass
			if (LoadingFinish)
			{
				Render::RenderDeferredLighting(Context, mSceneView.mMainConstants);
			}

			//After Lighting Pass
			{
				mPRTManager.DebugVisual(Context);
				Lighting::RenderAreaLightMesh(Context);
			}

			//Transparent Pass
			{
				ScopedTimer TransparentPassScope(L"Transparent Pass", Context);
				DefaultMeshPass.RenderMeshes(MeshRenderPass::eTransparent, Context, mSceneView.mMainConstants);
			}

		}
		Context.Finish();

#ifdef DEBUG
		EngineGUI::AddRenderPassVisualizeTexture("GBuffer", WStringToString(g_GBuffer[0].GetName()), g_GBuffer[0].GetHeight(), g_GBuffer[0].GetWidth(), g_GBuffer[0].GetSRV());
		EngineGUI::AddRenderPassVisualizeTexture("GBuffer", WStringToString(g_GBuffer[1].GetName()), g_GBuffer[1].GetHeight(), g_GBuffer[1].GetWidth(), g_GBuffer[1].GetSRV());
		EngineGUI::AddRenderPassVisualizeTexture("GBuffer", WStringToString(g_GBuffer[2].GetName()), g_GBuffer[2].GetHeight(), g_GBuffer[2].GetWidth(), g_GBuffer[2].GetSRV());
		EngineGUI::AddRenderPassVisualizeTexture("GBuffer", WStringToString(g_GBuffer[3].GetName()), g_GBuffer[3].GetHeight(), g_GBuffer[3].GetWidth(), g_GBuffer[3].GetSRV());
		EngineGUI::AddRenderPassVisualizeTexture("GBuffer", WStringToString(g_GBuffer[4].GetName()), g_GBuffer[4].GetHeight(), g_GBuffer[4].GetWidth(), g_GBuffer[4].GetSRV());
#endif // DEBUG
	}
}

void Scene::CreateTileConeShadowMap(GraphicsContext& Context)
{
	//Update a light's shadow every frame
	static UINT shadowIndex = 0;

	if (shadowIndex >= Lighting::MaxShadowedLights)
		return;

	ScopedTimer _prof(L"Tile Cone lighting Shadow Map", Context);

	MeshRenderPass shadowSorter(MeshRenderPass::eShadows);
	shadowSorter.SetCamera(Lighting::ConeShadowCamera[shadowIndex]);
	shadowSorter.SetDepthStencilTarget(*dynamic_cast<DepthBuffer*>(&Lighting::m_LightShadowTempBuffer));

	for (auto& model : m_pGLTFRenderObjects)
	{
		if (model->GetVisible() && model->GetShadowRenderFlag())
		{
			dynamic_cast<glTFInstanceModel*>(model)->GetModel()->AddToRender(shadowSorter);
		}
	}
	shadowSorter.Sort();
	shadowSorter.RenderMeshes(MeshRenderPass::eZPass, Context, mSceneView.mMainConstants);

	Context.TransitionResource(Lighting::m_LightShadowTempBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
	Context.TransitionResource(Lighting::m_LightShadowArray, D3D12_RESOURCE_STATE_COPY_DEST);
	Context.CopySubresource(Lighting::m_LightShadowArray, shadowIndex, Lighting::m_LightShadowTempBuffer, 0);
	Context.TransitionResource(Lighting::m_LightShadowArray, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	++shadowIndex;
}





