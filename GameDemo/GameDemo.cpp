#include "EngineUtility.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "TextureManager.h"
#include "ConstantBuffer.h"
#include "EngineInput.h"
#include "BufferManager.h"
#include "EngineGUI.h"
#include "resource.h"

#include "Camera.h"
#include "Sky.h"
#include "ModelLoader.h"
#include "SceneManager.h"

//lib
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib,"dxguid.lib")

using namespace GlareEngine;
using namespace GlareEngine::GameCore;
using namespace GlareEngine::DirectX12Graphics;
using namespace GlareEngine::EngineInput;

#define MAXSRVSIZE 256

const int gNumFrameResources = 3;

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
	void BuildRootSignature();

	void BuildPSO();

	void UpdateWindow(float DeltaTime);

	void UpdateCamera(float DeltaTime);

	void UpdateMainConstantBuffer(float DeltaTime);

	void InitializeScene();
protected:
	// 处理鼠标输入的重载函数
	virtual void OnMouseDown(WPARAM btnState, int x, int y);
	virtual void OnMouseUp(WPARAM btnState, int x, int y);
	virtual void OnMouseMove(WPARAM btnState, int x, int y);
private:
	//Main Constant Buffer
	MainConstants mMainConstants;
	//Main Camera
	unique_ptr<Camera> mCamera;
	//Sky
	unique_ptr<CSky> mSky;
	//Root Signature
	RootSignature mRootSignature;
	//UI
	unique_ptr<EngineGUI> mEngineUI;

	//Viewport and Scissor
	D3D12_VIEWPORT m_MainViewport = { 0 };
	D3D12_RECT m_MainScissor = { 0 };
	
	float mCameraSpeed = 100.0f;

	D3D12_RECT mClientRect = { 0 };
	//scene manager
	unique_ptr<SceneManager> mSceneManager;

};


//////////////////////////////////////////////////////////////

//Game App entry
CREATE_APPLICATION(App);


//////////////////////////////////////////////////////////////
void App::InitializeScene()
{
	assert(mSceneManager);
	mSceneManager->CreateScene("Test Scene");
	//Create Materials Constant Buffer
	MaterialManager::GetMaterialInstance()->CreateMaterialsConstantBuffer();


}

void App::Startup(void)
{
	GraphicsContext& InitializeContext = GraphicsContext::Begin(L"Initialize");

	ID3D12GraphicsCommandList* CommandList = InitializeContext.GetCommandList();

	//Main Camera
	mCamera = make_unique<Camera>();
	//Sky Initialize
	mSky = make_unique<CSky>(CommandList, 5.0f, 20, 20);
	//UI Init
	mEngineUI = make_unique<EngineGUI>(CommandList);
	//Scene Manager
	mSceneManager = make_unique<SceneManager>(CommandList);

	InitializeScene();





	BuildRootSignature();
	BuildPSO(); 
	
	InitializeContext.Finish(true);
}

void App::Cleanup(void)
{
	mEngineUI->ShutDown();
	MaterialManager::Release();
	ModelLoader::Release();
}

void App::Update(float DeltaTime)
{
	UpdateWindow(DeltaTime);
	UpdateCamera(DeltaTime);
	UpdateMainConstantBuffer(DeltaTime);
}

void App::BuildRootSignature()
{
	mRootSignature.Reset(6, 8);
	//Main Constant Buffer
	mRootSignature[0].InitAsConstantBuffer(0);
	//Objects Constant Buffer
	mRootSignature[1].InitAsConstantBuffer(1);
	//Sky Cube Texture
	mRootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	//Cook BRDF PBR Textures 
	mRootSignature[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, MAXSRVSIZE);
	//Material Constant Data
	mRootSignature[4].InitAsBufferSRV(1, 1);
	//Instance Constant Data 
	mRootSignature[5].InitAsBufferSRV(0, 1);

	//Static Samplers
	mRootSignature.InitStaticSampler(0, SamplerLinearWrapDesc);
	mRootSignature.InitStaticSampler(1, SamplerAnisoWrapDesc);
	mRootSignature.InitStaticSampler(2, SamplerShadowDesc);
	mRootSignature.InitStaticSampler(3, SamplerLinearClampDesc);
	mRootSignature.InitStaticSampler(4, SamplerVolumeWrapDesc);
	mRootSignature.InitStaticSampler(5, SamplerPointClampDesc);
	mRootSignature.InitStaticSampler(6, SamplerPointBorderDesc);
	mRootSignature.InitStaticSampler(7, SamplerLinearBorderDesc);

	mRootSignature.Finalize(L"Forward Rendering", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
}

void App::BuildPSO()
{
	CSky::BuildPSO(mRootSignature);
	InstanceModel::BuildPSO(mRootSignature);
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
	if (EngineInput::IsPressed(GInput::kKey_W))
	{
		mCamera->Walk(mCameraSpeed * DeltaTime);
	}

	if (EngineInput::IsPressed(GInput::kKey_S))
	{
		mCamera->Walk(-mCameraSpeed * DeltaTime);
	}

	if (EngineInput::IsPressed(GInput::kKey_A))
	{
		mCamera->Strafe(-mCameraSpeed * DeltaTime);
	}

	if (EngineInput::IsPressed(GInput::kKey_D))
	{
		mCamera->Strafe(mCameraSpeed * DeltaTime);
	}

	//update camera matrix
	mCamera->UpdateViewMatrix();
}

void App::UpdateMainConstantBuffer(float DeltaTime)
{
	XMMATRIX View = XMLoadFloat4x4(&mCamera->GetView4x4f());
	XMMATRIX Proj = XMLoadFloat4x4(&mCamera->GetProj4x4f());

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
	//XMStoreFloat4x4(&mMainConstants.ShadowTransform, XMMatrixTranspose(XMLoadFloat4x4(&mShadowMap->mShadowTransform)));

	mMainConstants.EyePosW = mCamera->GetPosition3f();
	mMainConstants.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainConstants.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainConstants.NearZ = 1.0f;
	mMainConstants.FarZ = 1000.0f;
	mMainConstants.TotalTime = GameTimer::TotalTime();
	mMainConstants.DeltaTime = DeltaTime;

	//lights constant buffer
	{
		mMainConstants.gAmbientLight = { 0.25f, 0.25f, 0.25f, 1.0f };

		mMainConstants.Lights[0].Direction = XMFLOAT3(0.57735f, -0.47735f, 0.57735f);
		mMainConstants.Lights[0].Strength = { 1.0f, 1.0f, 1.0f };
		mMainConstants.Lights[0].FalloffStart = 1.0f;
		mMainConstants.Lights[0].FalloffEnd = 50.0f;
		mMainConstants.Lights[0].Position = { 20,20,20 };

		mMainConstants.Lights[1].Direction = XMFLOAT3(-0.57735f, -0.47735f, 0.57735f);
		mMainConstants.Lights[1].Strength = { 1.0f, 1.0f, 1.0f };
		mMainConstants.Lights[1].FalloffStart = 1.0f;
		mMainConstants.Lights[1].FalloffEnd = 50.0f;
		mMainConstants.Lights[1].Position = { 20,20,20 };

		mMainConstants.Lights[2].Direction = XMFLOAT3(0.57735f, -0.47735f, -0.57735f);
		mMainConstants.Lights[2].Strength = { 1.0f, 1.0f, 1.0f };
		mMainConstants.Lights[2].FalloffStart = 1.0f;
		mMainConstants.Lights[2].FalloffEnd = 50.0f;
		mMainConstants.Lights[2].Position = { 20,20,20 };
	}
}

void App::RenderScene(void)
{
	GraphicsContext& RenderContext = GraphicsContext::Begin(L"RenderScene");
#pragma region Scene
	RenderContext.PIXBeginEvent(L"Scene");

	RenderContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

	RenderContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	RenderContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

	RenderContext.ClearDepth(g_SceneDepthBuffer);
	RenderContext.ClearRenderTarget(g_SceneColorBuffer);

	RenderContext.SetRootSignature(mRootSignature);

	//set scene render target & Depth Stencli target
	RenderContext.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV());

	//set main constant buffer
	RenderContext.SetDynamicConstantBufferView(0, sizeof(mMainConstants), &mMainConstants);

	//Draw sky
#pragma region Draw sky
	RenderContext.PIXBeginEvent(L"Draw Sky");
	mSky->Draw(RenderContext);
	RenderContext.PIXEndEvent();
#pragma endregion

#pragma region Test Scene
	mSceneManager->GetScene("Test Scene")->RenderScene(RenderContext);
#pragma endregion


	RenderContext.PIXEndEvent();
#pragma endregion
	RenderContext.Finish(true);

}


void App::RenderUI()
{
	GraphicsContext& RenderContext = GraphicsContext::Begin(L"Render UI");
	//Draw UI
	RenderContext.PIXBeginEvent(L"Render UI");
	RenderContext.SetRenderTarget(GetCurrentBuffer().GetRTV());
	RenderContext.TransitionResource(GetCurrentBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	mEngineUI->Draw(RenderContext.GetCommandList());
	RenderContext.PIXEndEvent();
	RenderContext.TransitionResource(GetCurrentBuffer(), D3D12_RESOURCE_STATE_PRESENT, true);
	RenderContext.Finish(true);
}

void App::OnResize(uint32_t width, uint32_t height)
{

	DirectX12Graphics::Resize(width, height);

	//窗口调整大小，因此更新宽高比并重新计算投影矩阵;
	mCamera->SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 40000.0f);

	m_MainViewport.Width = (float)g_SceneColorBuffer.GetWidth();
	m_MainViewport.Height = (float)g_SceneColorBuffer.GetHeight();
	m_MainViewport.MinDepth = 0.0f;
	m_MainViewport.MaxDepth = 1.0f;

	m_MainScissor.left = 0;
	m_MainScissor.top = 0;
	m_MainScissor.right = (LONG)g_SceneColorBuffer.GetWidth();
	m_MainScissor.bottom = (LONG)g_SceneColorBuffer.GetHeight();


	//Client Rect
	mClientRect = { static_cast<LONG>(mClientWidth * CLIENT_FROMLEFT),
		static_cast<LONG>(MainMenuBarHeight),
		static_cast<LONG>(static_cast<float>(mClientWidth *(1-CLIENT_FROMLEFT))),
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
	if ((btnState & MK_LBUTTON) != 0)
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