#include "CommandContext.h"
#include "GraphicsCore.h"
#include "GameCore.h"
#include "L3DGameTimer.h"
#include "EngineInput.h"
//#include "BufferManager.h"
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
		
		using namespace GlareEngine::DirectX12Graphics;

		void InitializeApplication(GameApp& Game)
		{
			DirectX12Graphics::Initialize();
			EngineInput::Initialize();
			EngineAdjust::Initialize();

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

			float DeltaTime = DirectX12Graphics::GetFrameTime();

			EngineInput::Update(DeltaTime);
			EngineAdjust::Update(DeltaTime);

			Game.Update(DeltaTime);
			Game.RenderScene();
			//Game.RenderUI();
			//PostEffects::Render();

			DirectX12Graphics::Present();
			
			return !Game.IsDone();
		}


		// Default implementation to be overridden by the application
		bool GameApp::IsDone(void)
		{
			return EngineInput::IsFirstPressed(EngineInput::kKey_Escape);
		}


		HWND g_hWnd = nullptr;

		//void InitWindow(const wchar_t* className);
		LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

		void RunApplication(GameApp& app, const wchar_t* className,HINSTANCE hand)
		{
			//ASSERT_SUCCEEDED(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED));
			Microsoft::WRL::Wrappers::RoInitializeWrapper InitializeWinRT(RO_INIT_MULTITHREADED);
			ThrowIfFailed(InitializeWinRT);


			//当前所调用该函数的程序实例句柄
			HINSTANCE hInst = hand;

			// Register class
			WNDCLASSEX wcex;
			wcex.cbSize = sizeof(WNDCLASSEX);
			wcex.style = CS_HREDRAW | CS_VREDRAW;
			wcex.lpfnWndProc = WndProc;
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
				return ;
			}
			// Create window
			RECT rc = { 0, 0,1600,900/* (LONG)g_DisplayWidth, (LONG)g_DisplayHeight*/ };
			AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

			g_hWnd = CreateWindow(className, className, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
				rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);

			assert(g_hWnd != 0);

			InitializeApplication(app);
			
			ShowWindow(g_hWnd, SW_SHOW);
			UpdateWindow(g_hWnd);
			
			do
			{
				MSG msg = {};
				while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				if (msg.message == WM_QUIT)
					break;
				
			} while (UpdateApplication(app));    // Returns false to quit loop

			TerminateApplication(app);
			DirectX12Graphics::Terminate();
			DirectX12Graphics::Shutdown();
		}

		//--------------------------------------------------------------------------------------
		// Called every time the application receives a message
		//--------------------------------------------------------------------------------------
		LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
		{

			
			switch (message)
			{
			case WM_SIZE:
				//DirectX12Graphics::Resize((UINT)(UINT64)lParam & 0xFFFF, (UINT)(UINT64)lParam >> 16);
				break;

			case WM_DESTROY:
				PostQuitMessage(0);
				exit(0);
				break;

			default:
				return DefWindowProc(hWnd, message, wParam, lParam);
			}

			return 0;
		}


	}
}
using namespace GlareEngine::GameCore;

CREATE_APPLICATION(GameApp);