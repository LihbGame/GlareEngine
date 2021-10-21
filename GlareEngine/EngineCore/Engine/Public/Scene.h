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

	void SetShadowMap(ShadowMap* shadowMap) { m_pShadowMap = shadowMap; }
	void CreateModelInstance(string ModelName, int Num_X, int Num_Y);
	void CreateSimpleModelInstance(string ModelName, SimpleModelType Type, string MaterialName, int Num_X, int Num_Y);
private:
	void CreateShadowMap(GraphicsContext& Context);
	void ForwardRendering(GraphicsContext& Context);
	void ForwardPlusRendering(GraphicsContext& Context);
	void DeferredRendering(GraphicsContext& Context);
private:
	ID3D12GraphicsCommandList* m_pCommandList = nullptr;
	string mName;
	vector<InstanceModel> mModels;
	vector<RenderObject*> pRenderObjects;

	ShadowMap* m_pShadowMap = nullptr;
};

