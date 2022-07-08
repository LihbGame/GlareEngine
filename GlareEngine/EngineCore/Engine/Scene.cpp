#include "Scene.h"
#include "ModelLoader.h"
#include "Shadow/ShadowMap.h"
#include "EngineGUI.h"
#include "Render.h"

/// Scene/////////////////////////////////////////////

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
	for (auto &object: m_pRenderObjects)
	{
		object->Update(DeltaTime);
	}
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
		//update visible
		if (TypeVisible[object->mObjectType] != object->GetVisible())
		{
			object->SetVisible(TypeVisible[object->mObjectType]);
		}
		//update Model shadow visible
		if (object->mObjectType == ObjectType::Model && ShadowVisible != object->GetShadowFlag())
		{
			object->SetShadowFlag(ShadowVisible);
		}
	}
}

void Scene::BakingGIData(GraphicsContext& Context)
{
	mIBLGI.Initialize();
	mIBLGI.PreBakeGIData(Context, m_pRenderObjectsType[(int)ObjectType::Sky].front());
}

void Scene::SetSceneLights(Light* light)
{
	int CopySize = sizeof(light) * sizeof(Light);
	memcpy_s(mSceneLights, CopySize, light, CopySize);
}

void Scene::SetRootSignature(RootSignature* rootSignature)
{
	m_pRootSignature = rootSignature;
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
	case ObjectType::Shadow:{
		m_pShadowMap = dynamic_cast<ShadowMap*>(Object);
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
	RenderContext.Finish(true);
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
		mMainConstants.Lights[0].FalloffStart =mSceneLights[0].FalloffStart;
		mMainConstants.Lights[0].FalloffEnd = mSceneLights[0].FalloffEnd;
		mMainConstants.Lights[0].Position = mSceneLights[0].Position;

		mMainConstants.Lights[1].Direction = mSceneLights[1].Direction;
		mMainConstants.Lights[1].Strength = mSceneLights[1].Strength;
		mMainConstants.Lights[1].FalloffStart = mSceneLights[1].FalloffStart;
		mMainConstants.Lights[1].FalloffEnd = mSceneLights[1].FalloffEnd;
		mMainConstants.Lights[1].Position = mSceneLights[1].Position;

		mMainConstants.Lights[2].Direction = mSceneLights[2].Direction;
		mMainConstants.Lights[2].Strength = mSceneLights[2].Strength;
		mMainConstants.Lights[2].FalloffStart = mSceneLights[2].FalloffStart;
		mMainConstants.Lights[2].FalloffEnd = mSceneLights[2].FalloffEnd;
		mMainConstants.Lights[2].Position = mSceneLights[2].Position;
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

	//set main constant buffer
	Context.SetDynamicConstantBufferView((int)RootSignatureType::eMainConstantBuffer, sizeof(mMainConstants), &mMainConstants);


	//Set Cube SRV
	Context.SetDynamicDescriptors((int)RootSignatureType::eCubeTextures, 0, (UINT)g_CubeSRV.size(), g_CubeSRV.data());
	//Set Textures SRV
	Context.SetDynamicDescriptors((int)RootSignatureType::ePBRTextures, 0, (UINT)g_TextureSRV.size(), g_TextureSRV.data());
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
		Context.TransitionResource(g_SceneMSAAColorBuffer, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, true);
		Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RESOLVE_DEST, true);
		Context.GetCommandList()->ResolveSubresource(g_SceneColorBuffer.GetResource(), 0, g_SceneMSAAColorBuffer.GetResource(), 0, DefaultHDRColorFormat);
	}

	Context.PIXEndEvent();
#pragma endregion
	Context.Finish(true);
}

void Scene::ForwardPlusRendering()
{
}

void Scene::DeferredRendering()
{
}





