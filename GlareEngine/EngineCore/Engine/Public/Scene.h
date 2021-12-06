#pragma once
#include "InstanceModel.h"
#include "SimpleModelGenerator.h"
#include "ShadowMap.h"
#include "IBL.h"
class Scene
{
public:
	Scene(string name, ID3D12GraphicsCommandList* pCommandList);
	~Scene() {};
public:
	//Update Scene 
	void Update(float DeltaTime);
	//Add Objects to Scene
	void AddObjectToScene(RenderObject* Object);
	//Render Scene
	void RenderScene(RenderPipelineType Type,GraphicsContext& Context);
	//Release Scene
	void ReleaseScene();
	//Resize Viewport and Scissor
	void ResizeViewport(uint32_t width, uint32_t height);
	//Set Shadow Class Type
	void SetShadowMap(ShadowMap* shadowMap) { m_pShadowMap = shadowMap; }
	//Update objects visible
	void VisibleUpdateForType(unordered_map<ObjectType, bool> TypeVisible);
	//Baking Scene's Global illumination data
	void BakingGIData(GraphicsContext& Context);
public:
	bool IsWireFrame = false;
	bool IsMSAA = false;
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

	vector<RenderObject*> m_pRenderObjects;
	vector<RenderObject*> m_pRenderObjectsType[(int)ObjectType::Count];

	ShadowMap* m_pShadowMap = nullptr;
	//IBL Global illumination
	IBL mIBLGI;

};

