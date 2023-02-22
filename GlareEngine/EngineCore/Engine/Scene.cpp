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

/// Scene/////////////////////////////////////////////
using namespace GlareEngine::Render;


Scene::Scene(string name, ID3D12GraphicsCommandList* pCommandList)
	:mName(name),
	m_pCommandList(pCommandList)
{
}

void Scene::Update(float DeltaTime)
{

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
	Context.Finish();

}

void Scene::VisibleUpdateForType()
{
	unordered_map<ObjectType, bool> TypeVisible;
	TypeVisible[ObjectType::Sky] = m_pGUI->IsShowSky();
	TypeVisible[ObjectType::Model] = m_pGUI->IsShowModel();
	TypeVisible[ObjectType::Shadow] = m_pGUI->IsShowShadow();

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
	case RenderPipelineType::Forward:
	{
		ForwardRendering();
		break;
	}

	case RenderPipelineType::Forward_Plus:
	{
		ForwardPlusRendering();
		break;
	}
	case RenderPipelineType::Deferred:
	{
		DeferredRendering();
		break;
	}
	default:
		break;
	}
	
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
	XMMATRIX View = XMLoadFloat4x4(&m_pCamera->GetView4x4f());
	XMMATRIX Proj = XMLoadFloat4x4(&m_pCamera->GetProj4x4f());

	XMMATRIX ViewProj = XMMatrixMultiply(View, Proj);
	XMMATRIX InvView = XMMatrixInverse(&XMMatrixDeterminant(View), View);
	XMMATRIX InvProj = XMMatrixInverse(&XMMatrixDeterminant(Proj), Proj);
	XMMATRIX InvViewProj = XMMatrixInverse(&XMMatrixDeterminant(ViewProj), ViewProj);

	XMStoreFloat4x4(&mMainConstants.View, XMMatrixTranspose(View));
	XMStoreFloat4x4(&mMainConstants.InvView, XMMatrixTranspose(InvView));
	XMStoreFloat4x4(&mMainConstants.Proj, XMMatrixTranspose(Proj));
	XMStoreFloat4x4(&mMainConstants.InvProj, XMMatrixTranspose(InvProj));
	XMStoreFloat4x4(&mMainConstants.ViewProj, XMMatrixTranspose(ViewProj));
	XMStoreFloat4x4(&mMainConstants.InvViewProj, XMMatrixTranspose(InvViewProj));
	XMStoreFloat4x4(&mMainConstants.ShadowTransform, XMMatrixTranspose(XMLoadFloat4x4(&m_pShadowMap->GetShadowTransform())));
	
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
	mMainConstants.TileCount[0] = Math::DivideByMultiple(g_SceneColorBuffer.GetWidth(), Lighting::LightGridDimension) + 1.0f;
	mMainConstants.TileCount[1] = Math::DivideByMultiple(g_SceneColorBuffer.GetHeight(), Lighting::LightGridDimension) + 1.0f;
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
		Context.GetCommandList()->ResolveSubresource(g_SceneColorBuffer.GetResource(), 0, g_SceneMSAAColorBuffer.GetResource(), 0, DefaultHDRColorFormat);
	}

	Context.PIXEndEvent();
#pragma endregion
	Context.Finish();
}

void Scene::ForwardPlusRendering()
{
	GraphicsContext& Context = GraphicsContext::Begin(L"Scene Render");

	// Begin rendering depth
	//MSAA
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
	


	for (auto& model : m_pGLTFRenderObjects)
	{
		if (model->GetVisible())
		{
			dynamic_cast<glTFInstanceModel*>(model)->GetModel()->AddToRender(sorter);
		}
	}

	sorter.Sort();

	///Depth PrePass
	{
		ScopedTimer _prof(L"Depth PrePass", Context);
		sorter.RenderMeshes(MeshSorter::eZPass, Context, mMainConstants);
	}


	if (LoadingFinish)
	{
		//Set lighting buffers in the SRV Heap
		if (IsMSAA)
		{
			Context.TransitionResource(g_SceneMSAADepthBuffer, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
			Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_RESOLVE_DEST, true);
			Context.GetCommandList()->ResolveSubresource(g_SceneDepthBuffer.GetResource(), 0, g_SceneMSAADepthBuffer.GetResource(), 0, DXGI_FORMAT_R32_FLOAT);
		}
		Lighting::FillLightGrid(Context, *m_pCamera);

		D3D12_CPU_DESCRIPTOR_HANDLE LightSRV[] = {
		Lighting::m_LightGrid.GetSRV(),
		Lighting::m_LightBuffer.GetSRV()
		};

		UINT destCount = 2;
		UINT size[2] = { 1,1 };
		g_Device->CopyDescriptors(1, &gTextureHeap[0], &destCount,
			destCount, LightSRV, size, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	//set descriptor heap 
	Context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, gTextureHeap.GetHeapPointer());
	//Set Common SRV
	Context.SetDescriptorTable((int)RootSignatureType::eCommonSRVs, gTextureHeap[0]);

	{
		ScopedTimer _outerprof(L"Main Render", Context);

		{
			ScopedTimer _prof(L"Sun Shadow Map", Context);

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
		

		{
			ScopedTimer _prof(L"Render Color", Context);
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

			//sky
			if (m_pRenderObjectsType[(int)ObjectType::Sky].front()->GetVisible())
			{
				Context.SetDynamicConstantBufferView((int)RootSignatureType::eMainConstantBuffer, sizeof(MainConstants), &mMainConstants);
				m_pRenderObjectsType[(int)ObjectType::Sky].front()->Draw(Context);
			}

			sorter.RenderMeshes(MeshSorter::eOpaque, Context, mMainConstants);
			
		}
		sorter.RenderMeshes(MeshSorter::eTransparent, Context, mMainConstants);
	}

	//MSAA
	if (IsMSAA)
	{
		Context.TransitionResource(g_SceneMSAAColorBuffer, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
		Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RESOLVE_DEST, true);
		Context.GetCommandList()->ResolveSubresource(g_SceneColorBuffer.GetResource(), 0, g_SceneMSAAColorBuffer.GetResource(), 0, DefaultHDRColorFormat);
	}

	Context.Finish();
}


void Scene::DeferredRendering()
{
}





