#include "Scene.h"
#include "ModelLoader.h"
#include "ScreenGrab.h"
#include "CommandListManager.h"
/// Scene/////////////////////////////////////////////

Scene::Scene(string name, ID3D12GraphicsCommandList* pCommandList)
	:mName(name),
	m_pCommandList(pCommandList)
{
}


void Scene::Update(float DeltaTime)
{

}

void Scene::VisibleUpdateForType(unordered_map<ObjectType, bool> TypeVisible)
{
	bool ShadowVisible = TypeVisible[ObjectType::Shadow];
	for (auto& object : pRenderObjects)
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
	


	//MSAA
	if (IsMSAA)
	{
		Context.TransitionResource(g_SceneMSAAColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		Context.TransitionResource(g_SceneMSAADepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
		Context.ClearDepth(g_SceneMSAADepthBuffer);
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
	for (auto& RenderObject : pRenderObjects)
	{
		if (RenderObject->GetVisible())
		{
			Context.PIXBeginEvent(RenderObject->GetName().c_str());
			RenderObject->Draw(Context);
			Context.PIXEndEvent();
		}
	}
	Context.PIXEndEvent();


	//SaveDDSTextureToFile(g_CommandManager.GetQueue(D3D12_COMMAND_LIST_TYPE_DIRECT).GetCommandQueue(), g_SceneColorBuffer.m_pResource.Get(),
		//L"dd.dds",D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	//MSAA
	if (IsMSAA)
	{
		Context.TransitionResource(g_SceneMSAAColorBuffer, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, true);
		Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RESOLVE_DEST, true);
		Context.GetCommandList()->ResolveSubresource(g_SceneColorBuffer.GetResource(), 0, g_SceneMSAAColorBuffer.GetResource(), 0, DefaultHDRColorFormat);
	}

}

void Scene::ForwardPlusRendering(GraphicsContext& Context)
{
}

void Scene::DeferredRendering(GraphicsContext& Context)
{
}





