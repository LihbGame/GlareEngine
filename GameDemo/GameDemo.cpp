#include "EngineUtility.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "TextureManager.h"
#include "ConstantBuffer.h"
#include "EngineInput.h"

#include "Camera.h"
#include "CSky.h"

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

public:
	void BuildRootSignature();

	void BuildPSO();

	void UpdateCamera(float DeltaTime);

	void UpdateMainConstantBuffer(float DeltaTime);




private:
	//Main Constant Buffer
	MainConstants mMainConstants;
	//Main Camera
	unique_ptr<Camera> mCamera;
	//Sky
	unique_ptr<CSky> mSky;
	//Root Signature
	RootSignature mRootSignature;


	float mCameraSpeed = 100.0f;
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



	BuildRootSignature();
	BuildPSO();
	


	InitializeContext.Finish(true);
}

void App::Cleanup(void)
{
}

void App::Update(float DeltaTime)
{
	UpdateCamera(DeltaTime);
	UpdateMainConstantBuffer(DeltaTime);


}

void App::BuildRootSignature()
{
	mRootSignature.Reset(2, 2);
	mRootSignature[0].InitAsBufferSRV(0, D3D12_SHADER_VISIBILITY_PIXEL);
	mRootSignature[1].InitAsConstantBuffer(0);
	mRootSignature.InitStaticSampler(0, SamplerLinearClampDesc);
	mRootSignature.InitStaticSampler(1, SamplerPointClampDesc);
	mRootSignature.Finalize(L"Render");
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

	RenderContext.Finish(true);
}

void App::OnResize(uint32_t width, uint32_t height)
{
	DirectX12Graphics::Resize(width, height);

	//窗口调整大小，因此更新宽高比并重新计算投影矩阵;
	mCamera->SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 40000.0f);


}
