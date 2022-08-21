﻿#include "EngineUtility.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "TextureManager.h"
#include "ConstantBuffer.h"
#include "EngineInput.h"
#include "BufferManager.h"
#include "SamplerManager.h"
#include "EngineGUI.h"
#include "EngineLog.h"
#include "resource.h"

#include "Camera.h"
#include "ModelLoader.h"
#include "SceneManager.h"
#include "Sky/Sky.h"
#include "Shadow/ShadowMap.h"
#include "Terrain/Terrain.h"
#include "Render.h"

//lib
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib,"dxguid.lib")

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

	void CreateModelInstance(ID3D12GraphicsCommandList* CommandList, std::string ModelName, int Num_X, int Num_Y);

	void CreateSimpleModelInstance(ID3D12GraphicsCommandList* CommandList, std::string ModelName, SimpleModelType Type, std::string MaterialName, int Num_X, int Num_Y);
protected:
	// 处理鼠标输入的重载函数
	virtual void OnMouseDown(WPARAM btnState, int x, int y);
	virtual void OnMouseUp(WPARAM btnState, int x, int y);
	virtual void OnMouseMove(WPARAM btnState, int x, int y);
private:
	//Only one scene
	Scene* gScene = nullptr;
	//Scene data 
	std::vector<std::unique_ptr<InstanceModel>> mModels;
	//Main Camera
	std::unique_ptr<Camera> mCamera;
	//Sky
	std::unique_ptr<CSky> mSky;
	//Shadow map
	std::unique_ptr<ShadowMap> mShadowMap;
	//UI
	std::unique_ptr<EngineGUI> mEngineUI;
	
	float mCameraSpeed = CAMERA_SPEED;

	D3D12_RECT mClientRect = { 0 };
	//scene manager
	std::unique_ptr<SceneManager> mSceneManager;

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
	mCamera = std::make_unique<Camera>(REVERSE_Z);
	mCamera->LookAt(XMFLOAT3(-200, 200, 200), XMFLOAT3(0, 0, 0), XMFLOAT3(0, 1, 0));
	//Sky Initialize
	mSky = std::make_unique<CSky>(CommandList, 5.0f, 20, 20);
	//Shadow map Initialize
	mShadowMap = std::make_unique<ShadowMap>(XMFLOAT3(0.57735f, -0.47735f, 0.57735f), SHADOWMAPSIZE, SHADOWMAPSIZE);
	//Create PBR Materials
	SimpleModelGenerator::GetInstance(CommandList)->CreatePBRMaterials();

	//Initialize Render
	Render::Initialize();


	//////Build Scene//////////
	assert(mSceneManager);
	gScene = mSceneManager->CreateScene("Test Scene");
	auto InitScene = [&]()
	{
		Light SceneLights[3];
		SceneLights[0].Direction = mShadowMap->GetShadowedLightDir();
		SceneLights[0].Strength = { 1.0f, 1.0f, 1.0f };
		SceneLights[0].FalloffStart = 1.0f;
		SceneLights[0].FalloffEnd = 50.0f;
		SceneLights[0].Position = { 20,20,20 };

		SceneLights[1].Direction = XMFLOAT3(-0.57735f, -0.47735f, 0.57735f);
		SceneLights[1].Strength = { 1.0f, 1.0f, 1.0f };
		SceneLights[1].FalloffStart = 1.0f;
		SceneLights[1].FalloffEnd = 50.0f;
		SceneLights[1].Position = { 20,20,20 };

		SceneLights[2].Direction = XMFLOAT3(0.57735f, -0.47735f, -0.57735f);
		SceneLights[2].Strength = { 1.0f, 1.0f, 1.0f };
		SceneLights[2].FalloffStart = 1.0f;
		SceneLights[2].FalloffEnd = 50.0f;
		SceneLights[2].Position = { 20,20,20 };

		assert(gScene);
		//set global scene
		EngineGlobal::gCurrentScene = gScene;
		//set Root Signature
		gScene->SetRootSignature(&gRootSignature);
		//Set UI
		gScene->SetSceneUI(mEngineUI.get());
		//set camera
		gScene->SetCamera(mCamera.get());
		//set scene lights
		gScene->SetSceneLights(SceneLights);
		//set Shadow map 
		gScene->SetShadowMap(mShadowMap.get());
		//add hdr Sky
		gScene->AddObjectToScene(mSky.get());

		InitializeContext.Flush(true);
		//Release Upload Temporary Textures (must flush GPU command wait for texture already load in GPU)
		TextureManager::GetInstance(CommandList)->ReleaseUploadTextures();

		//Load and Initialize Models in work threads
		mEngineThread.AddTask([&]() {
			GraphicsContext& InitializeContext = GraphicsContext::Begin(L"Load and Initialize Models");
			ID3D12GraphicsCommandList* CommandList = InitializeContext.GetCommandList();
			//Instance models
			CreateSimpleModelInstance(CommandList, "Grid_01", SimpleModelType::Grid, "PBRGrass01", 1, 1);
			CreateModelInstance(CommandList, "BlueTree/Blue_Tree_02a.FBX", 5, 5);
			//CreateSimpleModelInstance(CommandList,"Sphere_01", SimpleModelType::Sphere, "PBRrocky_shoreline1", 1, 1);
			//CreateSimpleModelInstance(CommandList,"Sphere_01", SimpleModelType::Sphere, "PBRRock046S", 1, 1);
			//CreateModelInstance(CommandList,"TraumaGuard/TraumaGuard.FBX", 5, 5);

			//add models
			for (auto& model : mModels)
			{
				gScene->AddObjectToScene(model.get());
			}

			InitializeContext.Finish(true);

			//Release Upload Temporary Textures (must flush GPU command wait for texture already load in GPU)
			TextureManager::GetInstance(CommandList)->ReleaseUploadTextures();

			//Create Model Materials Constant Buffer
			MaterialManager::GetMaterialInstance()->CreateMaterialsConstantBuffer();
			});

		//Baking GI Data
		gScene->BakingGIData(InitializeContext);

		//Create all Materials Constant Buffer
		MaterialManager::GetMaterialInstance()->CreateMaterialsConstantBuffer();

	};
	
	InitScene();

	//mEngineThread.Flush();
}

void App::Startup(void)
{
	GraphicsContext& InitializeContext = GraphicsContext::Begin(L"Initialize");
	ID3D12GraphicsCommandList* CommandList = InitializeContext.GetCommandList();
	//UI Init
	mEngineUI = std::make_unique<EngineGUI>(CommandList);
	//Scene Manager
	mSceneManager = std::make_unique<SceneManager>(CommandList);
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
	gScene->Update(DeltaTime);
}

void App::UpdateSceneState(float DeltaTime)
{
	assert(gScene);
	gScene->VisibleUpdateForType();

	bool IsStateChange = false;
	if (mEngineUI->IsWireframe() != gScene->IsWireFrame)
	{
		gScene->IsWireFrame = mEngineUI->IsWireframe();
		gCommonProperty.IsWireframe = gScene->IsWireFrame;
		IsStateChange = true;
	}
	if (mEngineUI->IsMSAA() != gScene->IsMSAA)
	{
		gScene->IsMSAA = mEngineUI->IsMSAA();
		gCommonProperty.IsMSAA = gScene->IsMSAA;
		if (gScene->IsMSAA)
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
	//if (!mMaximized)
	//{
	//	SendMessage(g_hWnd, WM_SYSCOMMAND, SC_MAXIMIZE, NULL);
	//}
	//if (mMaximized)
	//{
	//	SendMessage(g_hWnd, WM_SYSCOMMAND, SC_RESTORE, NULL);
	//}
}

void App::UpdateCamera(float DeltaTime)
{
	float x = 0.0f, y = 0.0f, z = 0.0f;
	
	mCameraSpeed = mEngineUI->GetCameraModeSpeed();

	if (EngineInput::IsPressed(GInput::kKey_W))
	{
		z = mCameraSpeed / DeltaTime;
	}

	if (EngineInput::IsPressed(GInput::kKey_S))
	{
		z = -mCameraSpeed / DeltaTime;
	}

	if (EngineInput::IsPressed(GInput::kKey_A))
	{
		x = -mCameraSpeed / DeltaTime;
	}

	if (EngineInput::IsPressed(GInput::kKey_D))
	{
		x = mCameraSpeed / DeltaTime;
	}

	mCamera->Move(x, y, z);
	mEngineUI->SetCameraPosition(mCamera->GetPosition3f());

	//update camera matrix
	mCamera->UpdateViewMatrix();
}


void App::RenderScene(void)
{
	gScene->RenderScene(RenderPipelineType::Forward);
}


void App::RenderUI()
{
	if (!mIsHideUI)
	{
		gScene->DrawUI();
	}
}

void App::OnResize(uint32_t width, uint32_t height)
{
	Display::Resize(width, height);

	//窗口调整大小，因此更新宽高比并重新计算投影矩阵;
	mCamera->SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, FAR_Z);
	//resize Viewport and Scissor
	gScene->ResizeViewport(width, height);

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
			SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
			mCursorType = CursorType::SIZENWSE;
			return;
		}
		if (mLastMousePos.y >= ((LONG)WindowRect.bottom - RESIZE_RANGE) &&
			mLastMousePos.y <= ((LONG)WindowRect.bottom))
		{
			SetCursor(LoadCursor(NULL, IDC_SIZENS));
			mCursorType = CursorType::SIZENS;
		}
		else if (mLastMousePos.x >= ((LONG)WindowRect.right - RESIZE_RANGE) &&
			mLastMousePos.x <= ((LONG)WindowRect.right))
		{
			SetCursor(LoadCursor(NULL, IDC_SIZEWE));
			mCursorType = CursorType::SIZEWE;
		}
	}
}


void App::CreateModelInstance(ID3D12GraphicsCommandList* CommandList, std::string ModelName, int Num_X, int Num_Y)
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
				XMStoreFloat4x4(&IRC.mWorldTransform, XMMatrixTranspose(/*XMMatrixRotationX(MathHelper::Pi/2) * */XMMatrixRotationY(MathHelper::RandF() * MathHelper::Pi) * XMMatrixScaling(0.2f, 0.2f, 0.2f) * XMMatrixTranslation((i - Num_X / 2) * 50.0f, 0, (y - Num_Y / 2) * 50.0f)));
				InstanceData.mInstanceConstants[SubMeshIndex].push_back(IRC);
			}
		}
	}
	auto lModel = std::make_unique<InstanceModel>(StringToWString(ModelName), InstanceData);
	lModel->SetShadowFlag(true);
	mModels.push_back(std::move(lModel));
}

void App::CreateSimpleModelInstance(ID3D12GraphicsCommandList* CommandList, std::string ModelName, SimpleModelType Type, std::string MaterialName, int Num_X, int Num_Y)
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
				XMStoreFloat4x4(&IRC.mWorldTransform, XMMatrixTranspose(XMMatrixScaling(4, 4, 4) * XMMatrixTranslation(i * 60.0f, 0, y * 60.0f)));
				XMStoreFloat4x4(&IRC.mTexTransform, XMMatrixTranspose(XMMatrixScaling(3, 3, 3)));
				InstanceData.mInstanceConstants[SubMeshIndex].push_back(IRC);
			}
		}
	}
	auto lModel = std::make_unique<InstanceModel>(StringToWString(ModelName), InstanceData);
	//lModel->SetShadowFlag(true);
	mModels.push_back(std::move(lModel));
}