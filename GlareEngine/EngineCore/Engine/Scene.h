
#include <typeinfo>
#include "GI/IBL.h"
#include "GI/PRTManager.h"

class ShadowMap;
class EngineGUI;

using Render::RenderPipelineType;

typedef void (*BuildPSO)(PSOCommonProperty);

class SceneView
{
public:
	SceneView() {};

public:
	//Main Constant Buffer
	MainConstants mMainConstants;

	//View camera
	Camera* m_pCamera = nullptr;

	//Viewport and Scissor
	D3D12_VIEWPORT m_MainViewport = { 0 };
	D3D12_RECT m_MainScissor = { 0 };

	//ref scene
	Scene* m_pScene = nullptr;
};



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
		m_pSunShadowCamera->SetPosition(mSceneView.m_pCamera->GetPosition3f());
	}
	//Set Camera 
	void SetCamera(Camera* camera) { mSceneView.m_pCamera = camera; }
	//Get Camera
	Camera* GetCamera() { return mSceneView.m_pCamera; }
	//Set UI
	void SetSceneUI(EngineGUI* UI) { m_pGUI = UI; }
	//Update objects visible
	void VisibleUpdateForType();
	//Baking Scene's Global illumination data
	void BakingGIData(GraphicsContext& Context);
	//Set Scene lights
	void SetSceneLights(DirectionalLight* light, int DirectionalLightsCount = 1);
	//Set RootSignature
	void SetRootSignature(RootSignature* rootSignature);
	RootSignature* GetRootSignature() { return m_pRootSignature; }
	//Shadow Map
	void CreateShadowMap(GraphicsContext& Context, vector<RenderObject*> RenderObjects);
	void CreateShadowMapForGLTF(GraphicsContext& Context);

	vector<RenderObject*> GetRenderObjects() { return m_pGLTFRenderObjects; }
	vector<RenderObject*> GetRenderObjects(ObjectType objectType) { return m_pRenderObjectsType[(int)objectType]; }

	SceneView GetSceneView() { return mSceneView; }

	void LinearizeZ(GraphicsContext& Context);

	void Finalize();
public:
	unique_ptr<ShadowCamera> m_pSunShadowCamera = nullptr;

	bool LoadingFinish = false;

	EngineGUI* m_pGUI = nullptr;

	bool IsWireFrame = false;
	bool IsMSAA = false;
private:
	void UpdateMainConstantBuffer(float DeltaTime);
	void ForwardRendering();
	void ForwardRendering(RenderPipelineType renderPipeline);
	void DeferredRendering(RenderPipelineType renderPipeline);
	void CreateTileConeShadowMap(GraphicsContext& Context);
private:
	ID3D12GraphicsCommandList* m_pCommandList = nullptr;
	string mName;

	SceneView mSceneView;

	vector<RenderObject*> m_pRenderObjects;
	vector<RenderObject*> m_pGLTFRenderObjects;
	vector<RenderObject*> m_pRenderObjectsType[(int)ObjectType::Count];

	AxisAlignedBox mSceneAABB;

	ShadowMap* m_pShadowMap = nullptr;

	//IBL Global illumination
	IBL mIBLGI;

	//scene lights
	DirectionalLight mSceneLights[MAX_DIRECTIONAL_LIGHTS];

	RootSignature* m_pRootSignature;

	PRTManager mPRTManager;
};

