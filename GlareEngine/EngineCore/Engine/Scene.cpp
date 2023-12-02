#include "Scene.h"
#include "Animation/ModelLoader.h"
#include "Shadow/ShadowMap.h"
#include "EngineGUI.h"
#include "Graphics/Render.h"
#include "Model/Model.h"
#include "InstanceModel/glTFInstanceModel.h"
#include "Model/MeshSorter.h"
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

	//Update Screen Processing
	ScreenProcessing::Update(DeltaTime, mMainConstants, *m_pCamera);

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
	mMainConstants.gDirectionalLightsCount = DirectionalLightsCount;
	int CopySize = DirectionalLightsCount * sizeof(DirectionalLight);
	memcpy_s(mSceneLights, CopySize, light, CopySize);

	if (mName == "Sponza")
	{
		mMainConstants.gIsIndoorScene = true;
		mSceneLights[0].Strength = { 10.0f, 10.0f, 10.0f };
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
	dynamic_cast<glTFInstanceModel*>(Object)->GetModel()->LoopAllAnimations();
	m_pGLTFRenderObjects.push_back(Object);
}

void Scene::RenderScene(RenderPipelineType Type)
{
	switch (Type)
	{
	case RenderPipelineType::FR:
	{
		ForwardRendering();
		break;
	}
	case RenderPipelineType::TBFR:
	{
		TiledBaseForwardRendering();
		break;
	}
	case RenderPipelineType::TBDR:
	{
		TiledBaseDeferredRendering();
		break;
	}
	default:
		break;
	}

	//Post Processing
	ScreenProcessing::Render(*m_pCamera);
}

void Scene::DrawUI()
{
	GraphicsContext& RenderContext = GraphicsContext::Begin(L"Render UI");
	//Draw UI
	RenderContext.PIXBeginEvent(L"Render UI");
	RenderContext.SetRenderTarget(Display::GetCurrentBuffer().GetRTV());
	RenderContext.TransitionResource(Display::GetCurrentBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	m_pGUI->BeginDraw(RenderContext.GetCommandList());

	for (auto& RenderObject : m_pRenderObjects)
	{
		if (RenderObject->GetVisible())
		{
			RenderObject->DrawUI();
		}
	}


	//Post Processing UI
	ScreenProcessing::DrawUI();

	m_pGUI->EndDraw(RenderContext.GetCommandList());
	RenderContext.PIXEndEvent();
	RenderContext.TransitionResource(Display::GetCurrentBuffer(), D3D12_RESOURCE_STATE_PRESENT, true);
	RenderContext.Finish();
}

void Scene::ReleaseScene()
{
}

void Scene::ResizeViewport(uint32_t width, uint32_t height)
{
	m_MainViewport.Width = (float)g_SceneColorBuffer.GetWidth();
	m_MainViewport.Height = (float)g_SceneColorBuffer.GetHeight();
	m_MainViewport.MinDepth = 0.0f;
	m_MainViewport.MaxDepth = 1.0f;

	m_MainScissor.left = 0;
	m_MainScissor.top = 0;
	m_MainScissor.right = (LONG)g_SceneColorBuffer.GetWidth();
	m_MainScissor.bottom = (LONG)g_SceneColorBuffer.GetHeight();
}


void Scene::UpdateMainConstantBuffer(float DeltaTime)
{
	XMFLOAT4X4 TempFloat4x4 = m_pCamera->GetView4x4f();
	XMMATRIX View = XMLoadFloat4x4(&TempFloat4x4);
	TempFloat4x4 = m_pCamera->GetProj4x4f();
	XMMATRIX Proj = XMLoadFloat4x4(&TempFloat4x4);

	XMMATRIX ViewProj = XMMatrixMultiply(View, Proj);
	XMVECTOR Determinant = XMMatrixDeterminant(View);
	XMMATRIX InvView = XMMatrixInverse(&Determinant, View);
	Determinant = XMMatrixDeterminant(Proj);
	XMMATRIX InvProj = XMMatrixInverse(&Determinant, Proj);
	Determinant = XMMatrixDeterminant(ViewProj);
	XMMATRIX InvViewProj = XMMatrixInverse(&Determinant, ViewProj);

	XMStoreFloat4x4(&mMainConstants.View, XMMatrixTranspose(View));
	XMStoreFloat4x4(&mMainConstants.InvView, XMMatrixTranspose(InvView));
	XMStoreFloat4x4(&mMainConstants.Proj, XMMatrixTranspose(Proj));
	XMStoreFloat4x4(&mMainConstants.InvProj, XMMatrixTranspose(InvProj));
	XMStoreFloat4x4(&mMainConstants.ViewProj, XMMatrixTranspose(ViewProj));
	XMStoreFloat4x4(&mMainConstants.InvViewProj, XMMatrixTranspose(InvViewProj));
	XMFLOAT4X4 ShadowTransform = m_pShadowMap->GetShadowTransform();
	XMStoreFloat4x4(&mMainConstants.ShadowTransform, XMMatrixTranspose(XMLoadFloat4x4(&ShadowTransform)));
	
	mMainConstants.EyePosW = m_pCamera->GetPosition3f();
	mMainConstants.RenderTargetSize = XMFLOAT2((float)g_SceneColorBuffer.GetWidth(), (float)g_SceneColorBuffer.GetHeight());
	mMainConstants.InvRenderTargetSize = XMFLOAT2(1.0f / g_SceneColorBuffer.GetWidth(), 1.0f / g_SceneColorBuffer.GetHeight());
	mMainConstants.NearZ = m_pCamera->GetNearZ();
	mMainConstants.FarZ = m_pCamera->GetFarZ();
	mMainConstants.TotalTime = GameTimer::TotalTime();
	mMainConstants.DeltaTime = DeltaTime;

	//Tiled light data
	mMainConstants.InvTileDim[0] = 1.0f / Lighting::LightGridDimension;
	mMainConstants.InvTileDim[1] = 1.0f / Lighting::LightGridDimension;
	mMainConstants.TileCount[0] = Math::DivideByMultiple(g_SceneColorBuffer.GetWidth(), Lighting::LightGridDimension);
	mMainConstants.TileCount[1] = Math::DivideByMultiple(g_SceneColorBuffer.GetHeight(), Lighting::LightGridDimension);
	mMainConstants.FirstLightIndex[0] = Lighting::m_FirstConeLight;
	mMainConstants.FirstLightIndex[1] = Lighting::m_FirstConeShadowedLight;

	//Shadow Map 
	mMainConstants.mShadowMapIndex = m_pShadowMap->GetShadowMapIndex();

	assert(GlobleSRVIndex::gSkyCubeSRVIndex >= 0);
	assert(GlobleSRVIndex::gBakingDiffuseCubeIndex >= 0);
	assert(GlobleSRVIndex::gBakingPreFilteredEnvIndex >= 0);
	assert(GlobleSRVIndex::gBakingIntegrationBRDFIndex >= 0);
	//GI DATA
	mMainConstants.mSkyCubeIndex = GlobleSRVIndex::gSkyCubeSRVIndex;
	mMainConstants.mBakingDiffuseCubeIndex = GlobleSRVIndex::gBakingDiffuseCubeIndex;
	mMainConstants.gBakingPreFilteredEnvIndex = GlobleSRVIndex::gBakingPreFilteredEnvIndex;
	mMainConstants.gBakingIntegrationBRDFIndex = GlobleSRVIndex::gBakingIntegrationBRDFIndex;

	//Lights constant buffer
	{
		mMainConstants.gAmbientLight = { 0.05f, 0.05f, 0.05f, 1.0f };

		mMainConstants.Lights[0].Direction = mSceneLights[0].Direction;
		mMainConstants.Lights[0].Strength = mSceneLights[0].Strength;

		mMainConstants.Lights[1].Direction = mSceneLights[1].Direction;
		mMainConstants.Lights[1].Strength = mSceneLights[1].Strength;

		mMainConstants.Lights[2].Direction = mSceneLights[2].Direction;
		mMainConstants.Lights[2].Strength = mSceneLights[2].Strength;
	}

}


void Scene::CreateShadowMap(GraphicsContext& Context,vector<RenderObject*> RenderObjects)
{
	assert(m_pShadowMap);
	m_pShadowMap->Draw(Context,RenderObjects);
}

void Scene::ForwardRendering()
{
	GraphicsContext& Context = GraphicsContext::Begin(L"RenderScene");
#pragma region Scene
	Context.PIXBeginEvent(L"Scene");

	Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

	Context.ClearDepthAndStencil(g_SceneDepthBuffer, REVERSE_Z ? 0.0f : 1.0f);

	Context.ClearRenderTarget(g_SceneColorBuffer);

	Context.SetRootSignature(*m_pRootSignature);

	Context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, gTextureHeap.GetHeapPointer());

	//set main constant buffer
	Context.SetDynamicConstantBufferView((int)RootSignatureType::eMainConstantBuffer, sizeof(mMainConstants), &mMainConstants);
	//Set Cube SRV
	Context.SetDescriptorTable((int)RootSignatureType::eCubeTextures, gTextureHeap[COMMONSRVSIZE]);
	//Set Textures SRV
	Context.SetDescriptorTable((int)RootSignatureType::ePBRTextures, gTextureHeap[MAXCUBESRVSIZE+ COMMONSRVSIZE]);
	//Set Material Data
	const vector<MaterialConstant>& MaterialData = MaterialManager::GetMaterialInstance()->GetMaterialsConstantBuffer();
	Context.SetDynamicSRV((int)RootSignatureType::eMaterialConstantData, sizeof(MaterialConstant) * MaterialData.size(), MaterialData.data());

	Context.PIXBeginEvent(L"Shadow Pass");
	CreateShadowMap(Context,m_pRenderObjects);
	Context.PIXEndEvent();

	//set Viewport And Scissor
	Context.SetViewportAndScissor(m_MainViewport, m_MainScissor);
	


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

		ScreenProcessing::LinearizeZ(computeContext, *m_pCamera, frameIndex);
	}

	//SSAO
	{
		SSAO::Render(Context, mMainConstants);
	}


	Context.PIXEndEvent();
#pragma endregion
	Context.Finish();
}

void Scene::TiledBaseForwardRendering()
{
	GraphicsContext& Context = GraphicsContext::Begin(L"Scene Render");

	//default batch
	MeshSorter sorter(MeshSorter::eDefault);
	sorter.SetCamera(*m_pCamera);
	sorter.SetViewport(m_MainViewport);
	sorter.SetScissor(m_MainScissor);

	if (IsMSAA)
	{
		sorter.SetDepthStencilTarget(g_SceneMSAADepthBuffer);
		sorter.AddRenderTarget(g_SceneMSAAColorBuffer);
	}
	else
	{ 
		sorter.SetDepthStencilTarget(g_SceneDepthBuffer);
		sorter.AddRenderTarget(g_SceneColorBuffer);
	}
	
	//Culling and add  Render Objects
	for (auto& model : m_pGLTFRenderObjects)
	{
		if (model->GetVisible())
		{
			dynamic_cast<glTFInstanceModel*>(model)->GetModel()->AddToRender(sorter);
		}
	}

	//Sort Visible objects
	sorter.Sort();

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

		sorter.RenderMeshes(MeshSorter::eZPass, Context, mMainConstants);

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

		ScreenProcessing::LinearizeZ(computeContext, *m_pCamera, frameIndex);
	}

	//SSAO
	{
		SSAO::Render(Context, mMainConstants);
	}


	if (LoadingFinish)
	{
		//Light Culling
		Lighting::FillLightGrid(Context, *m_pCamera);

		//Copy PBR SRV
		D3D12_CPU_DESCRIPTOR_HANDLE PBR_SRV[] = 
		{
		Lighting::m_LightGrid.GetSRV(),
		Lighting::m_LightBuffer.GetSRV(),
		Lighting::m_LightShadowArray.GetSRV(),
		g_SSAOFullScreen.GetSRV()
		};

		UINT destCount = 4; UINT size[4] = { 1,1,1,1 };
		g_Device->CopyDescriptors(1, &gTextureHeap[0], &destCount,
			destCount, PBR_SRV, size, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

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
		{
			ScopedTimer SunShadowScope(L"Sun Shadow Map", Context);

			MeshSorter shadowSorter(MeshSorter::eShadows);
			shadowSorter.SetCamera(*m_pSunShadowCamera.get());
			shadowSorter.SetDepthStencilTarget(m_pSunShadowCamera->GetShadowMap()->GetShadowBuffer());

			for (auto& model : m_pGLTFRenderObjects)
			{
				if (model->GetVisible() && model->GetShadowRenderFlag())
				{
					dynamic_cast<glTFInstanceModel*>(model)->GetModel()->AddToRender(shadowSorter);
				}
			}
			shadowSorter.Sort();
			shadowSorter.RenderMeshes(MeshSorter::eZPass, Context, mMainConstants);
		}

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
			Context.SetDescriptorTable((int)RootSignatureType::eCubeTextures, gTextureHeap[COMMONSRVSIZE]);
			//Set Textures SRV
			Context.SetDescriptorTable((int)RootSignatureType::ePBRTextures, gTextureHeap[MAXCUBESRVSIZE + COMMONSRVSIZE]);

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
			Context.SetViewportAndScissor(m_MainViewport, m_MainScissor);

			//Sky
			if (m_pRenderObjectsType[(int)ObjectType::Sky].front()->GetVisible())
			{
				Context.SetDynamicConstantBufferView((int)RootSignatureType::eMainConstantBuffer, sizeof(MainConstants), &mMainConstants);
				m_pRenderObjectsType[(int)ObjectType::Sky].front()->Draw(Context);
			}

			sorter.RenderMeshes(MeshSorter::eOpaque, Context, mMainConstants);
			
		}

		//Transparent Pass
		sorter.RenderMeshes(MeshSorter::eTransparent, Context, mMainConstants);

		//Resolve MSAA
		if (IsMSAA)
		{
			Context.TransitionResource(g_SceneMSAAColorBuffer, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
			Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RESOLVE_DEST, true);
			Context.GetCommandList()->ResolveSubresource(g_SceneColorBuffer.GetResource(), 0, g_SceneMSAAColorBuffer.GetResource(), 0, DefaultHDRColorFormat);
		}
	}

	Context.Finish();
}


void Scene::TiledBaseDeferredRendering()
{



}

void Scene::CreateTileConeShadowMap(GraphicsContext& Context)
{
	static UINT shadowIndex = 0;

	if (shadowIndex >= Lighting::MaxShadowedLights)
		return;

	ScopedTimer _prof(L"Tile Cone lighting Shadow Map", Context);

	MeshSorter shadowSorter(MeshSorter::eShadows);
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
	shadowSorter.RenderMeshes(MeshSorter::eZPass, Context, mMainConstants);

	Context.TransitionResource(Lighting::m_LightShadowTempBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
	Context.TransitionResource(Lighting::m_LightShadowArray, D3D12_RESOURCE_STATE_COPY_DEST);
	Context.CopySubresource(Lighting::m_LightShadowArray, shadowIndex, Lighting::m_LightShadowTempBuffer, 0);
	Context.TransitionResource(Lighting::m_LightShadowArray, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	++shadowIndex;
}





