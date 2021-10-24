#pragma once
#include "InstanceModel.h"
#include "SimpleModelGenerator.h"
#include "ShadowMap.h"
class Scene
{
public:
	Scene(string name, ID3D12GraphicsCommandList* pCommandList);
	~Scene() {};
	void Update(float DeltaTime);
	void AddObjectToScene(RenderObject* Object);
	void RenderScene(RenderPipelineType Type,GraphicsContext& Context);
	void ReleaseScene();
	//resize Viewport and Scissor
	void ResizeViewport(uint32_t width, uint32_t height);
	void SetShadowMap(ShadowMap* shadowMap) { m_pShadowMap = shadowMap; }
	void VisibleUpdateForType(unordered_map<ObjectType, bool> TypeVisible);
private:
	void CreateShadowMap(GraphicsContext& Context,vector<RenderObject*> RenderObjects);
	void ForwardRendering(GraphicsContext& Context);
	void ForwardPlusRendering(GraphicsContext& Context);
	void DeferredRendering(GraphicsContext& Context);
private:
	ID3D12GraphicsCommandList* m_pCommandList = nullptr;
	string mName;

	//Viewport and Scissor
	D3D12_VIEWPORT m_MainViewport = { 0 };
	D3D12_RECT m_MainScissor = { 0 };

	vector<RenderObject*> pRenderObjects;

	ShadowMap* m_pShadowMap = nullptr;
};

