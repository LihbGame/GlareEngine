#include "CommandContext.h"
#include "GraphicsCore.h"
#include "GameCore.h"
#include "L3DGameTimer.h"
#include "EngineInput.h"
#include "BufferManager.h"
#include "EngineAdjust.h"
#include "EngineProfiling.h"
#include <windowsx.h>
#include "resource.h"
#pragma comment(lib, "runtimeobject.lib")

namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		//extern ColorBuffer g_GenMipsBuffer;
	}
}



namespace GlareEngine
{
	namespace GameCore
	{
		GameApp* GameApp::mGameApp = nullptr;
		HWND g_hWnd = nullptr;

		LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			//转发hwnd因为我们可以在CreateWindow返回之前获取消息(例如，WM_CREATE)。
			return GameApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
		}

		void InitializeApplication(GameApp& Game)
		{
			//Core Initialize
			DirectX12Graphics::Initialize();
			EngineInput::Initialize();
			EngineAdjust::Initialize();
			GameTimer::Reset();

			//Game Initialize
			Game.Startup();
		}

		void TerminateApplication(GameApp& Game)
		{
			Game.Cleanup();
			EngineInput::Shutdown();
		}

		bool UpdateApplication(GameApp& Game)
		{
			EngineProfiling::Update();
			GameTimer::Tick();

			float DeltaTime = DirectX12Graphics::GetFrameTime();

			EngineInput::Update(DeltaTime);
			EngineAdjust::Update(DeltaTime);

			Game.Update(DeltaTime);
			Game.RenderScene();
			//PostEffects::Render();

			DirectX12Graphics::Present();
			
			return !Game.IsDone();
		}


		GameApp::GameApp()
		{
			// Only one GameApp can be constructed.
			assert(mGameApp == nullptr);
			mGameApp = this;
		}

		// Default implementation to be overridden by the application
		bool GameApp::IsDone(void)
		{
			return EngineInput::IsFirstPressed(EngineInput::kKey_Escape);
		}

		float GameApp::AspectRatio() const
		{
			return static_cast<float>(mClientWidth) / mClientHeight;
		}


		//void InitWindow(const wchar_t* className);
		LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
		static bool gExit = false;

		void RunApplication(GameApp& app, const wchar_t* className, HINSTANCE hand)
		{
			// Enable run-time memory check for debug builds.
//#if defined(DEBUG) | defined(_DEBUG)
//			_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
//#endif


			//ASSERT_SUCCEEDED(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED));
			Microsoft::WRL::Wrappers::RoInitializeWrapper InitializeWinRT(RO_INIT_MULTITHREADED);
			ThrowIfFailed(InitializeWinRT);


			//当前所调用该函数的程序实例句柄
			HINSTANCE hInst = hand;

			// Register class
			WNDCLASSEX wcex;
			wcex.cbSize = sizeof(WNDCLASSEX);
			wcex.style = CS_HREDRAW | CS_VREDRAW;
			wcex.lpfnWndProc = MainWndProc;
			wcex.cbClsExtra = 0;
			wcex.cbWndExtra = 0;
			wcex.hInstance = hInst;
			wcex.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));
			wcex.hCursor = LoadCursor(hInst, MAKEINTRESOURCE(IDC_CURSOR1));
			wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
			wcex.lpszMenuName = nullptr;
			wcex.lpszClassName = className;
			wcex.hIconSm = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));
			if (!RegisterClassEx(&wcex))
			{
				DWORD dwerr = GetLastError();
				MessageBox(0, L"RegisterClass Failed.", 0, 0);
				return;
			}
			// Create window
			RECT rc = { 0, 0,(LONG)g_DisplayWidth, (LONG)g_DisplayHeight };
			AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

			g_hWnd = CreateWindow(className, className, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
				rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);

			assert(g_hWnd != 0);


			//初始化
			InitializeApplication(app);

			ShowWindow(g_hWnd, SW_SHOW);
			UpdateWindow(g_hWnd);

			do
			{
				MSG msg = {};
				while(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				if (msg.message == WM_QUIT)
					break;

			} while (UpdateApplication(app) && !gExit);    // Returns false to quit loop
			
			DirectX12Graphics::Terminate();
			TerminateApplication(app);
			DirectX12Graphics::Shutdown();

		}

		//--------------------------------------------------------------------------------------
		// Called every time the application receives a message
		//--------------------------------------------------------------------------------------
		LRESULT GameApp::MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			switch (message)
			{
			case WM_SIZE:
			{
				// Save the new client area dimensions.
				mClientWidth = LOWORD(lParam);
				mClientHeight = HIWORD(lParam);
				OnResize(mClientWidth, mClientHeight);
				break;
			}
			case WM_LBUTTONDOWN:
			case WM_MBUTTONDOWN:
			case WM_RBUTTONDOWN:
				OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				break;
			case WM_LBUTTONUP:
			case WM_MBUTTONUP:
			case WM_RBUTTONUP:
				OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				break;
			case WM_MOUSEMOVE:
				OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				break;
			case WM_DESTROY:
			{
				PostQuitMessage(0); 
				gExit = true;
				break;
			}
			default:
				return DefWindowProc(hWnd, message, wParam, lParam);
			}

			return 0;
		}


	}
}