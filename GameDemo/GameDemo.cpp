﻿#include "EngineUtility.h"
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
	virtual void RenderUI() {};

	virtual void OnResize(uint32_t width, uint32_t height);

	//Msg 
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

public:
	void BuildRootSignature();

	void BuildPSO();

	void UpdateCamera(float DeltaTime);

	void UpdateMainConstantBuffer(float DeltaTime);
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
	D3D12_VIEWPORT m_MainViewport;
	D3D12_RECT m_MainScissor;
	
	float mCameraSpeed = 100.0f;

	POINT mLastMousePos;

	D3D12_RECT mClientRect;
};


//////////////////////////////////////////////////////////////

//Game App entry
CREATE_APPLICATION(App);

//////////////////////////////////////////////////////////////


void App::Startup(void)
{
	GraphicsContext& InitializeContext = GraphicsContext::Begin(L"Initialize");

	ID3D12GraphicsCommandList* CommandList = InitializeContext.GetCommandList();

	g_TextureManager.SetCommandList(CommandList);
	//Main Camera
	mCamera = make_unique<Camera>();
	//Sky Initialize
	mSky = make_unique<CSky>(CommandList, 5.0f, 20, 20);
	//UI Init
	mEngineUI = make_unique<EngineGUI>(g_hWnd, g_Device);


	BuildRootSignature();
	BuildPSO(); 
	

	InitializeContext.Finish(true);
}

void App::Cleanup(void)
{
	mEngineUI->ShutDown();
}

void App::Update(float DeltaTime)
{
	if (EngineGUI::mWindowMaxSize && !mMaximized)
	{
		SendMessage(g_hWnd, WM_SYSCOMMAND, SC_MAXIMIZE, NULL);
	}
	if (!EngineGUI::mWindowMaxSize && mMaximized)
	{
		SendMessage(g_hWnd, WM_SYSCOMMAND, SC_RESTORE, NULL);
	}


	UpdateCamera(DeltaTime);
	UpdateMainConstantBuffer(DeltaTime);

}

void App::BuildRootSignature()
{
	mRootSignature.Reset(3, 2);
	mRootSignature[0].InitAsConstantBuffer(0);
	mRootSignature[1].InitAsConstantBuffer(1);
	mRootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	mRootSignature.InitStaticSampler(0, SamplerLinearWrapDesc);
	mRootSignature.InitStaticSampler(1, SamplerLinearClampDesc);
	mRootSignature.Finalize(L"Render", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
}

void App::BuildPSO()
{
	mSky->BuildPSO(mRootSignature);
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
}

void App::RenderScene(void)
{
	GraphicsContext& RenderContext = GraphicsContext::Begin(L"RenderScene");

	RenderContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

	RenderContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	RenderContext.ClearRenderTarget(g_SceneColorBuffer);

	RenderContext.SetRootSignature(mRootSignature);
	//set render target
	RenderContext.SetRenderTarget(g_SceneColorBuffer.GetRTV());
	//set main constant buffer
	RenderContext.SetDynamicConstantBufferView(0, sizeof(mMainConstants), &mMainConstants);

	//Draw sky
	mSky->Draw(RenderContext);
	
	//Draw UI
	mEngineUI->Draw(RenderContext.GetCommandList());


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
			mLastMousePos.x = x;
			mLastMousePos.y = y;
		}
	}
}