#include "Engine/EngineUtility.h"
#include "Engine/GameCore.h"
#include "Graphics/GraphicsCore.h"
#include "Graphics/CommandContext.h"
#include "Graphics/TextureManager.h"
#include "Misc/ConstantBuffer.h"
#include "Engine/EngineInput.h"
#include "Graphics/BufferManager.h"
#include "Graphics/SamplerManager.h"
#include "EngineGUI.h"
#include "Engine/EngineLog.h"
#include "resource.h"

#include "Engine/Camera.h"
#include "Animation/ModelLoader.h"
#include "Engine/SceneManager.h"
#include "Sky/Sky.h"
#include "Shadow/ShadowMap.h"
#include "Terrain/Terrain.h"
#include "Graphics/Render.h"
#include "Model/Model.h"
#include "Model/glTFModelLoader.h"
#include "InstanceModel/glTFInstanceModel.h"
#include "Misc/LightManager.h"
#include "PostProcessing/PostProcessing.h"
#include "Engine/Tools/RuntimePSOManager.h"

//lib
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxguid.lib")

using namespace GlareEngine;
using namespace GlareEngine::GameCore;
using namespace GlareEngine::EngineInput;
using namespace GlareEngine::Render;


class App :public GameApp
{
public:
	virtual ~App() {}

	//此功能可用于初始化应用程序状态，并将在分配必要的硬件资源后运行。 
	//不依赖于这些资源的某些状态仍应在构造函数中初始化，例如指针和标志。
	virtual void Startup(void);// = 0;
	virtual void Cleanup(void);// = 0;

	//确定您是否要退出该应用。 默认情况下，应用程序继续运行，直到按下“ ESC”键为止。
	//virtual bool IsDone(void);

	//每帧将调用一次update方法。 状态更新和场景渲染都应使用此方法处理。
	virtual void Update(float DeltaTime);

	//rendering pass
	virtual void RenderScene(void);// = 0;

	//可选的UI（覆盖）渲染阶段。 这是LDR。 缓冲区已被清除。 
	virtual void RenderUI();

	virtual void OnResize(uint32_t width, uint32_t height);

	//Msg 
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

public:
	void UpdateSceneState(float DeltaTime);

	void UpdateWindow(float DeltaTime);

	void UpdateCamera(float DeltaTime);

	void InitializeScene(ID3D12GraphicsCommandList* CommandList, GraphicsContext& InitializeContext);

	void CreateModelInstance(ID3D12GraphicsCommandList* CommandList, string ModelName, int Num_X, int Num_Y,float offset, bool RotX = false);

	void CreateSimpleModelInstance(ID3D12GraphicsCommandList* CommandList,string ModelName, SimpleModelType Type, string MaterialName, int Num_X, int Num_Y);

	void LoadGITFModel(wstring modelName, float SizeScale, bool IsNegetiveZForward = true);
protected:
	// 处理鼠标输入的重载函数
	virtual void OnMouseDown(WPARAM btnState, int x, int y);
	virtual void OnMouseUp(WPARAM btnState, int x, int y);
	virtual void OnMouseMove(WPARAM btnState, int x, int y);
private:
	//scene
	vector<Scene*> gScenes;
	//Scene data 
	vector<unique_ptr<InstanceModel>> mModels;
	vector<unique_ptr<glTFInstanceModel>> mGLTFModels;
	//Main Camera
	unique_ptr<Camera> mCamera;
	//Sky
	unique_ptr<CSky> mSky;
	//Shadow map
	unique_ptr<ShadowMap> mShadowMap;
	//UI
	unique_ptr<EngineGUI> mEngineUI;
	
	float mCameraSpeed = CAMERA_SPEED;

	D3D12_RECT mClientRect = { 0 };
	//scene manager
	unique_ptr<SceneManager> mSceneManager;

	//PSO Common Property
	//PSOCommonProperty mCommonProperty;

	//Root Signature
	//RootSignature mRootSignature;
};
//////////////////////////////////////////////////////////////

//Game App entry
CREATE_APPLICATION(App);


//////////////////////////////////////////////////////////////
void App::InitializeScene(ID3D12GraphicsCommandList* CommandList,GraphicsContext& InitializeContext)
{
	//Main Camera
	mCamera = make_unique<Camera>(REVERSE_Z);
	mCamera->LookAt(XMFLOAT3(300, 200, -30), XMFLOAT3(0, 0, 0), XMFLOAT3(0, 1, 0));
	//Sky Initialize
	mSky = make_unique<CSky>(CommandList, 5.0f, 20, 20);
	//Shadow map Initialize
	mShadowMap = make_unique<ShadowMap>(XMFLOAT3(0.17735f, -0.335f, 0.07735f), SHADOWMAPSIZE, SHADOWMAPSIZE);
	//Create PBR Materials
	SimpleModelGenerator::GetInstance(CommandList)->CreatePBRMaterials();

	//Initialize Render
	Render::Initialize(CommandList);

	//////Build Scene//////////
	assert(mSceneManager);
	auto InitScene = [&]()
	{
		DirectionalLight SceneLights[3];
		SceneLights[0].Direction = mShadowMap->GetShadowedLightDir();
		SceneLights[0].Strength = { 5.0f, 5.0f, 5.0f };

		SceneLights[1].Direction = XMFLOAT3(-0.57735f, -0.47735f, 0.57735f);
		SceneLights[1].Strength = { 0.5f, 0.5f, 0.5f };

		SceneLights[2].Direction = XMFLOAT3(0.57735f, -0.47735f, -0.57735f);
		SceneLights[2].Strength = { 0.5f, 0.5f, 0.5f };

		gScenes.push_back(mSceneManager->CreateScene("Sponza"));
		gScenes.push_back(mSceneManager->CreateScene("SciFi Helmet"));
		gScenes.push_back(mSceneManager->CreateScene("Damaged Helmet"));
		gScenes.push_back(mSceneManager->CreateScene("Flight Helmet"));
		gScenes.push_back(mSceneManager->CreateScene("Blue Tree"));


		mEngineUI->SetScenes(gScenes);

		//set global scene
		EngineGlobal::gCurrentScene = gScenes[0];

		for (auto& scene : gScenes)
		{
			//set Root Signature
			scene->SetRootSignature(&gRootSignature);
			//Set UI
			scene->SetSceneUI(mEngineUI.get());
			//set camera
			scene->SetCamera(mCamera.get());
			//set scene lights
			if (scene->GetName() == "Sponza")
			{
				scene->SetSceneLights(SceneLights);
			}
			else
			{
				scene->SetSceneLights(SceneLights, sizeof(SceneLights) / sizeof(DirectionalLight));
			}
			//set Shadow map 
			scene->SetShadowMap(mShadowMap.get());
			//add hdr Sky
			scene->AddObjectToScene(mSky.get());
		}
		InitializeContext.Flush(true);
		//Release Upload Temporary Textures (must flush GPU command wait for texture already load in GPU)
		TextureManager::GetInstance(CommandList)->ReleaseUploadTextures();

		//Load and Initialize Models in work threads
		EngineThread::Get().AddTask([&]() {
			GraphicsContext& InitializeContext = GraphicsContext::Begin(L"Load and Initialize Models");
			ID3D12GraphicsCommandList* CommandList = InitializeContext.GetCommandList();

			//Baking GI Data
			EngineGlobal::gCurrentScene->BakingGIData(InitializeContext);

			//GLTF models
			LoadGITFModel(L"SciFiHelmet/glTF/SciFiHelmet.gltf",80);
			LoadGITFModel(L"DamagedHelmet/glTF/DamagedHelmet.gltf", 100);
			LoadGITFModel(L"FlightHelmet/glTF/FlightHelmet.gltf",200);
			LoadGITFModel(L"Sponza/pbr/sponza2.gltf", 30);

			//FBX Instance Models
			CreateSimpleModelInstance(CommandList, "Grid_01", SimpleModelType::Grid, "PBRGrass01", 1, 1);
			CreateModelInstance(CommandList, "BlueTree/Blue_Tree_02a.FBX", 8, 8, 0);
			CreateModelInstance(CommandList, "BlueTree/Blue_Tree_03a.FBX", 8, 8, 10.0f, true);
			CreateModelInstance(CommandList, "BlueTree/Blue_Tree_03d.FBX", 8, 8, -10.0f, true);
			//CreateSimpleModelInstance(CommandList,"Sphere_01", SimpleModelType::Sphere, "PBRrocky_shoreline1", 1, 1);
			//CreateSimpleModelInstance(CommandList,"Sphere_01", SimpleModelType::Sphere, "PBRRock046S", 1, 1);
			//CreateModelInstance(CommandList,"TraumaGuard/TraumaGuard.FBX", 5, 5);


			//add gltf models
			/*for (auto& model : mGLTFModels)
			{
				gScene->AddGLTFModelToScene(model.get());
			}*/

			// Scene 1  sponza
			mGLTFModels[3]->SetMaskFlag(true);
			gScenes[0]->AddGLTFModelToScene(mGLTFModels[3].get());
			//Create random lights in Sponsa
			Lighting::CreateRandomLights(mGLTFModels[3].get()->GetModel()->GetAxisAlignedBox().GetMin(), mGLTFModels[3].get()->GetModel()->GetAxisAlignedBox().GetMax(), Vector3(150, 60, 150));
			gScenes[0]->Finalize();

			// Scene 2
			gScenes[1]->AddGLTFModelToScene(mGLTFModels[0].get()); gScenes[1]->Finalize();
			// Scene 3
			gScenes[2]->AddGLTFModelToScene(mGLTFModels[1].get()); gScenes[2]->Finalize();
			// Scene 4
			gScenes[3]->AddGLTFModelToScene(mGLTFModels[2].get()); gScenes[3]->Finalize();

			// Scene 5
			for (auto& model : mModels)
			{
				model->SetMaskFlag(true);
				gScenes[4]->AddObjectToScene(model.get());
			}
			gScenes[4]->Finalize();

			InitializeContext.Finish(true);

			//Release Upload Temporary Textures (must flush GPU command wait for texture already load in GPU)
			TextureManager::GetInstance(CommandList)->ReleaseUploadTextures();

			//Create Model Materials Constant Buffer
			MaterialManager::GetMaterialInstance()->CreateMaterialsConstantBuffer();

			//Copy Descriptors Texture Heap
			Render::CopyTextureHeap();

		});


		//Copy Descriptors Texture Heap
		Render::CopyTextureHeap();
		//Create all Materials Constant Buffer
		MaterialManager::GetMaterialInstance()->CreateMaterialsConstantBuffer();

	};

	InitScene();

#if USE_RUNTIME_PSO
	RuntimePSOManager::Get().StartRuntimePSOThread();
#endif
	//EngineThread::Get().Flush();
}

void App::Startup(void)
{
	GraphicsContext& InitializeContext = GraphicsContext::Begin(L"Initialize");
	ID3D12GraphicsCommandList* CommandList = InitializeContext.GetCommandList();
	//UI Init
	mEngineUI = make_unique<EngineGUI>(CommandList);
	//Scene Manager
	mSceneManager = make_unique<SceneManager>(CommandList);

	//Create scene
	InitializeScene(CommandList, InitializeContext);

	InitializeContext.Finish(true);
}

void App::Cleanup(void)
{
	Render::ShutDown();
	EngineLog::SaveLog();
	mEngineUI->ShutDown();
	MaterialManager::Release();
	ModelLoader::Release();
	SimpleModelGenerator::Release();
}

void App::Update(float DeltaTime)
{
	UpdateWindow(DeltaTime);
	UpdateCamera(DeltaTime);
	UpdateSceneState(DeltaTime);
	//update scene
	EngineGlobal::gCurrentScene->Update(DeltaTime);
}

void App::UpdateSceneState(float DeltaTime)
{
	if (gScenes[mEngineUI->GetSceneIndex()]->LoadingFinish)
		EngineGlobal::gCurrentScene = gScenes[mEngineUI->GetSceneIndex()];

	assert(EngineGlobal::gCurrentScene);
	EngineGlobal::gCurrentScene->VisibleUpdateForType();

	bool IsStateChange = false;
	if (mEngineUI->IsWireframe() != EngineGlobal::gCurrentScene->IsWireFrame)
	{
		EngineGlobal::gCurrentScene->IsWireFrame = mEngineUI->IsWireframe();
		gCommonProperty.IsWireframe = EngineGlobal::gCurrentScene->IsWireFrame;
		IsStateChange = true;
	}
	if (ScreenProcessing::IsMSAA() != EngineGlobal::gCurrentScene->IsMSAA)
	{
		EngineGlobal::gCurrentScene->IsMSAA = ScreenProcessing::IsMSAA();
		gCommonProperty.IsMSAA = EngineGlobal::gCurrentScene->IsMSAA;
		if (EngineGlobal::gCurrentScene->IsMSAA)
		{
			gCommonProperty.MSAACount = MSAACOUNT;
		}
		else
		{
			gCommonProperty.MSAACount = 1;
		}
		IsStateChange = true;
	}
	if (IsStateChange)
	{
		Render::BuildPSOs();
	}
}

void App::UpdateWindow(float DeltaTime)
{
	if (EngineGUI::mWindowMaxSize && !mMaximized)
	{
		SendMessage(g_hWnd, WM_SYSCOMMAND, SC_MAXIMIZE, NULL);
	}
	if (!EngineGUI::mWindowMaxSize && mMaximized)
	{
		SendMessage(g_hWnd, WM_SYSCOMMAND, SC_RESTORE, NULL);
	}
}

void App::UpdateCamera(float DeltaTime)
{
	mCameraSpeed = mEngineUI->GetCameraModeSpeed();

	if (EngineInput::IsPressed(GInput::kKey_W))
	{
		mCamera->Walk(mCameraSpeed);
	}

	if (EngineInput::IsPressed(GInput::kKey_S))
	{
		mCamera->Walk(-mCameraSpeed );
	}

	if (EngineInput::IsPressed(GInput::kKey_A))
	{
		mCamera->Strafe(-mCameraSpeed);
	}

	if (EngineInput::IsPressed(GInput::kKey_D))
	{
		mCamera->Strafe(mCameraSpeed);
	}

	if (EngineInput::IsPressed(GInput::kKey_Lcontrol))
	{
		mCamera->Down(mCameraSpeed);
	}

	if (EngineInput::IsPressed(GInput::kKey_Space))
	{
		mCamera->Up(mCameraSpeed);
	}

	mEngineUI->SetCameraPosition(mCamera->GetPosition3f());

	//update camera matrix
	mCamera->UpdateViewMatrix();

}


void App::RenderScene(void)
{
	if (EngineGlobal::gCurrentScene->GetName() == "Blue Tree")
	{
		EngineGlobal::gCurrentScene->RenderScene(RenderPipelineType::FR);
	}
	else
	{
		EngineGlobal::gCurrentScene->RenderScene(Render::gRenderPipelineType);
	}
}

void App::RenderUI()
{
	if (!mIsHideUI)
	{
		EngineGlobal::gCurrentScene->DrawUI();
	}
}

void App::OnResize(uint32_t width, uint32_t height)
{
	Display::Resize(width, height);

	//窗口调整大小，因此更新宽高比并重新计算投影矩阵;
	mCamera->SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, FAR_Z);
	//resize Viewport and Scissor
	for (auto& scene:gScenes)
	{
		scene->ResizeViewport(width, height);
	}

	//Client Rect
	mClientRect = { static_cast<LONG>(mClientWidth * CLIENT_FROMLEFT),
		static_cast<LONG>(MainMenuBarHeight),
		static_cast<LONG>(static_cast<float>(mClientWidth * (1 - CLIENT_FROMLEFT))),
		static_cast<LONG>(static_cast<float>(mClientHeight * CLIENT_HEIGHT))
	};
}


// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT App::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	//UI MSG
	ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
	return GameApp::MsgProc(hwnd, msg, wParam, lParam);
}


void App::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(g_hWnd);
}

void App::OnMouseUp(WPARAM btnState, int x, int y)
{
	
	ReleaseCapture();
}

void App::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_RBUTTON) != 0)
	{
		if (x >= mClientRect.left && y >= mClientRect.top
			&& y <= (mClientRect.top + (mClientRect.bottom - mClientRect.top))
			&& x <= (mClientRect.left + (mClientRect.right - mClientRect.left)))
		{
			float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
			float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
			mCamera->Pitch(dy);
			mCamera->RotateY(dx);
		}
	}
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	//设置缩放光标
	if (!mMaximized)
	{
		RECT WindowRect;
		GetClientRect(g_hWnd, &WindowRect);
		mCursorType = CursorType::Count;
		if (mLastMousePos.y >= ((LONG)WindowRect.bottom - RESIZE_RANGE) &&
			mLastMousePos.x >= ((LONG)WindowRect.right - RESIZE_RANGE))
		{
			SetCursor(GameCore::gCursorSizeNWSE);
			mCursorType = CursorType::SIZENWSE;
			return;
		}
		if (mLastMousePos.y >= ((LONG)WindowRect.bottom - RESIZE_RANGE) &&
			mLastMousePos.y <= ((LONG)WindowRect.bottom))
		{
			SetCursor(GameCore::gCursorSizeNS);
			mCursorType = CursorType::SIZENS;
		}
		else if (mLastMousePos.x >= ((LONG)WindowRect.right - RESIZE_RANGE) &&
			mLastMousePos.x <= ((LONG)WindowRect.right))
		{
			SetCursor(GameCore::gCursorSizeWE);
			mCursorType = CursorType::SIZEWE;
		}
	}
}


void App::CreateModelInstance(ID3D12GraphicsCommandList* CommandList,string ModelName, int Num_X, int Num_Y,float offset, bool RotX)
{
	assert(CommandList);
	const ModelRenderData* ModelData = ModelLoader::GetModelLoader(CommandList)->GetModelRenderData(ModelName);

	InstanceRenderData InstanceData;
	InstanceData.mModelData = ModelData;
	InstanceData.mInstanceConstants.resize(ModelData->mSubModels.size());
	for (int SubMeshIndex = 0; SubMeshIndex < ModelData->mSubModels.size(); SubMeshIndex++)
	{
		for (int i = 0; i < Num_X; ++i)
		{
			for (int y = 0; y < Num_Y; ++y)
			{
				InstanceRenderConstants IRC;
				IRC.mMaterialIndex = ModelData->mSubModels[SubMeshIndex].mMaterial->mMatCBIndex;
				float scale = MathHelper::RandF() * 0.3f;
				if (!RotX)
				{
					XMStoreFloat4x4(&IRC.mWorldTransform, XMMatrixTranspose(XMMatrixRotationY(MathHelper::RandF() * MathHelper::Pi) * XMMatrixScaling(scale, scale, scale) * XMMatrixTranslation((i - Num_X / 2) * 50.0f + offset, 0, (y - Num_Y / 2) * 50.0f + offset)));
				}
				else
				{
					XMStoreFloat4x4(&IRC.mWorldTransform, XMMatrixTranspose(XMMatrixRotationX(MathHelper::Pi/2) * XMMatrixRotationY(MathHelper::RandF() * MathHelper::Pi) * XMMatrixScaling(scale, scale, scale) * XMMatrixTranslation((i - Num_X / 2) * 50.0f + offset, 0, (y - Num_Y / 2) * 50.0f + offset)));
				}
				InstanceData.mInstanceConstants[SubMeshIndex].push_back(IRC);
			}
		}
	}
	auto lModel = make_unique<InstanceModel>(StringToWString(ModelName), InstanceData);
	lModel->SetShadowFlag(true);
	mModels.push_back(std::move(lModel));
}

void App::CreateSimpleModelInstance(ID3D12GraphicsCommandList* CommandList, string ModelName, SimpleModelType Type, string MaterialName, int Num_X, int Num_Y)
{
	assert(CommandList);
	const ModelRenderData* ModelData = SimpleModelGenerator::GetInstance(CommandList)->CreateSimpleModelRanderData(ModelName, Type, MaterialName);
	InstanceRenderData InstanceData;
	InstanceData.mModelData = ModelData;
	InstanceData.mInstanceConstants.resize(ModelData->mSubModels.size());
	for (int SubMeshIndex = 0; SubMeshIndex < ModelData->mSubModels.size(); SubMeshIndex++)
	{
		for (int i = 0; i < Num_X; ++i)
		{
			for (int y = 0; y < Num_Y; ++y)
			{
				InstanceRenderConstants IRC;

				IRC.mMaterialIndex = ModelData->mSubModels[SubMeshIndex].mMaterial->mMatCBIndex;
				XMStoreFloat4x4(&IRC.mWorldTransform, XMMatrixTranspose(XMMatrixScaling(5, 5, 5) * XMMatrixTranslation(i * 60.0f, 0, y * 60.0f)));
				XMStoreFloat4x4(&IRC.mTexTransform, XMMatrixTranspose(XMMatrixScaling(3, 3, 3)));
				InstanceData.mInstanceConstants[SubMeshIndex].push_back(IRC);
			}
		}
	}
	auto lModel = make_unique<InstanceModel>(StringToWString(ModelName), InstanceData); 
	lModel->SetShadowFlag(false);
	mModels.push_back(std::move(lModel));
}


void App::LoadGITFModel(wstring name, float SizeScale,bool IsNegetiveZForward)
{
	auto modelInstance = make_unique<ModelInstance>(GlareEngine::LoadModel(name), SizeScale, IsNegetiveZForward);
	auto GLTFModel = make_unique<glTFInstanceModel>(std::move(modelInstance));
	GLTFModel->SetShadowFlag(true);
	mGLTFModels.push_back(std::move(GLTFModel));
}