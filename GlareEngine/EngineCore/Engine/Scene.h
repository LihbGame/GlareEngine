#pragma once
#include "GI/IBL.h"
#include <typeinfo>

class ShadowMap;
class EngineGUI;

typedef void (*BuildPSO)(PSOCommonProperty);

class Scene
{
public:
	Scene(string name, ID3D12GraphicsCommandList* pCommandList);
	~Scene() {};
public:
	string GetName() { return mName; }
	void SetName(string name) { mName = name; }
	//Update Scene 
	void Update(float DeltaTime);
	//Add Objects to Scene
	void AddObjectToScene(RenderObject* Object);
	void AddGLTFModelToScene(RenderObject* Object);
	//Render Scene
	void RenderScene(RenderPipelineType Type);
	//Draw UI
	void DrawUI();
	//Release Scene
	void ReleaseScene();
	//Resize Viewport and Scissor
	void ResizeViewport(uint32_t width, uint32_t height);
	//Set Shadow
	void SetShadowMap(ShadowMap* shadowMap) {
		m_pShadowMap = shadowMap; 
		m_pSunShadowCamera = make_unique<ShadowCamera>(m_pShadowMap);
		m_pSunShadowCamera->SetPosition(m_pCamera->GetPosition3f());
	}
	//Set Camera 
	void SetCamera(Camera* camera) { m_pCamera = camera; }
	//Set UI
	void SetSceneUI(EngineGUI* UI) { m_pGUI = UI; }
	//Update objects visible
	void VisibleUpdateForType();
	//Baking Scene's Global illumination data
	void BakingGIData(GraphicsContext& Context);
	//Set Scene lights
	void SetSceneLights(Light* light, int DirectionalLightsCount = 1, int PointLightsCount = 0, int SpotLightsCount = 0);
	//Set RootSignature
	void SetRootSignature(RootSignature* rootSignature);
public:
	Camera* m_pCamera = nullptr;
	unique_ptr<ShadowCamera> m_pSunShadowCamera = nullptr;

	EngineGUI* m_pGUI = nullptr;

	bool IsWireFrame = false;
	bool IsMSAA = false;
private:
	void UpdateMainConstantBuffer(float DeltaTime);
	void CreateShadowMap(GraphicsContext& Context, vector<RenderObject*> RenderObjects);
	void ForwardRendering();
	void ForwardPlusRendering();
	void DeferredRendering();
private:
	ID3D12GraphicsCommandList* m_pCommandList = nullptr;
	string mName;

	//Main Constant Buffer
	MainConstants mMainConstants;

	//Viewport and Scissor
	D3D12_VIEWPORT m_MainViewport = { 0 };
	D3D12_RECT m_MainScissor = { 0 };

	vector<RenderObject*> m_pRenderObjects;
	vector<RenderObject*> m_pGLTFRenderObjects;
	vector<RenderObject*> m_pRenderObjectsType[(int)ObjectType::Count];

	ShadowMap* m_pShadowMap = nullptr;

	//IBL Global illumination
	IBL mIBLGI;

	//scene lights
	Light mSceneLights[MaxLights];

	RootSignature* m_pRootSignature;
};

