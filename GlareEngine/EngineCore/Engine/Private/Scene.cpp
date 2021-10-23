#include "Scene.h"
#include "ModelLoader.h"
/// Scene/////////////////////////////////////////////

Scene::Scene(string name, ID3D12GraphicsCommandList* pCommandList)
	:mName(name),
	m_pCommandList(pCommandList)
{
}


void Scene::Update(float DeltaTime)
{

}

void Scene::AddObjectToScene(RenderObject* Object)
{
	pRenderObjects.push_back(Object);
}

void Scene::RenderScene(RenderPipelineType Type, GraphicsContext& Context)
{
	switch (Type)
	{
	case RenderPipelineType::Forward:
	{
		ForwardRendering(Context);
		break;
	}

	case RenderPipelineType::Forward_Plus:
	{
		ForwardPlusRendering(Context);
		break;
	}
	case RenderPipelineType::Deferred:
	{
		DeferredRendering(Context);
		break;
	}
	default:
		break;
	}
	
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


void Scene::CreateShadowMap(GraphicsContext& Context,vector<RenderObject*> RenderObjects)
{
	assert(m_pShadowMap);
	m_pShadowMap->Draw(Context,RenderObjects);
}

void Scene::ForwardRendering(GraphicsContext& Context)
{
	//Set Textures SRV
	Context.SetDynamicDescriptors(3, 0, (UINT)g_TextureSRV.size(), g_TextureSRV.data());
	//Set Material Data
	const vector<MaterialConstant>& MaterialData = MaterialManager::GetMaterialInstance()->GetMaterialsConstantBuffer();
	Context.SetDynamicSRV(4, sizeof(MaterialConstant) * MaterialData.size(), MaterialData.data());

	Context.PIXBeginEvent(L"Shadow Pass");
	CreateShadowMap(Context,pRenderObjects);
	Context.PIXEndEvent();

	//set Viewport And Scissor
	Context.SetViewportAndScissor(m_MainViewport, m_MainScissor);
	//set scene render target & Depth Stencil target
	Context.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV());

	Context.PIXBeginEvent(L"Main Pass");
	for (auto& RenderObject : pRenderObjects)
	{
		Context.PIXBeginEvent(RenderObject->GetName().c_str());
		RenderObject->Draw(Context);
		Context.PIXEndEvent();
	}
	Context.PIXEndEvent();
}

void Scene::ForwardPlusRendering(GraphicsContext& Context)
{
}

void Scene::DeferredRendering(GraphicsContext& Context)
{
}





