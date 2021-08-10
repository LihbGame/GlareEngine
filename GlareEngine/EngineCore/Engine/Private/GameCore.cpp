#include "CommandContext.h"
#include "GraphicsCore.h"
#include "GameCore.h"
#include "GameTimer.h"
#include "EngineInput.h"
#include "BufferManager.h"
#include "EngineAdjust.h"
#include "EngineProfiling.h"
#include "EngineGUI.h"

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

		void InitializeWindow(HINSTANCE hand, const wchar_t* className)
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
			wcex.lpfnWndProc = MainWndProc;
			wcex.cbClsExtra = 0;
			wcex.cbWndExtra = 0;
			wcex.hInstance = hInst;
			wcex.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));
			wcex.hCursor = LoadCursor(hInst, MAKEINTRESOURCE(IDC_CURSOR1));
			wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
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
			AdjustWindowRect(&rc, WS_POPUPWINDOW, FALSE);

			g_hWnd = CreateWindow(className, className, WS_THICKFRAME | WS_MAXIMIZEBOX | WS_POPUPWINDOW, 100, 100,
				rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);

			assert(g_hWnd != 0);
		}

		void PreRender(GameApp& Game)
		{
			//HDR OR LDR
			DirectX12Graphics::PreparePresent();
			//RenderUI
			Game.RenderUI();
			//Present
			DirectX12Graphics::Present();
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
			Game.OnResize(Game.mClientWidth, Game.mClientHeight);
		}

		void TerminateApplication(GameApp& Game)
		{
			Game.Cleanup();
			EngineInput::Shutdown();
		}

		bool UpdateApplication(GameApp& Game)
		{
			GameTimer::Tick();
			float DeltaTime = DirectX12Graphics::GetFrameTime();

			EngineProfiling::Update();
			EngineInput::Update(DeltaTime);
			EngineAdjust::Update(DeltaTime);

			//Update Game
			Game.Update(DeltaTime);
			//RenderScene
			Game.RenderScene();

			//PostEffects::Render();

			//HDR OR LDR
			DirectX12Graphics::PreparePresent();
			//RenderUI
			Game.RenderUI();
			//Present
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
			return static_cast <float>(mClientWidth) / mClientHeight;
		}


		//void InitWindow(const wchar_t* className);
		LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
		static bool gExit = false;

		void RunApplication(GameApp& app, const wchar_t* className, HINSTANCE hand)
		{
			// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
			_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

			//初始化
			InitializeWindow(hand, className);
			InitializeApplication(app);
			PreRender(app);

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

			} while (UpdateApplication(app) && !gExit);    // Returns false to quit loop

			DirectX12Graphics::Terminate();
			TerminateApplication(app);
			DirectX12Graphics::Shutdown();

		}

		//--------------------------------------------------------------------------------------
		// Called every time the application receives a message
		//--------------------------------------------------------------------------------------6
		LRESULT GameApp::MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			switch (message)
			{
			case WM_NCCALCSIZE:
			{
				break;
			}
			case WM_SIZE:
			{
				// Save the new client area dimensions.
				mClientWidth = LOWORD(lParam);
				mClientHeight = HIWORD(lParam);

				if (s_SwapChain1 != nullptr)
				{
					if (wParam == SIZE_MINIMIZED)
					{
						mAppPaused = true;
						mMinimized = true;
						mMaximized = false;
						EngineGUI::mWindowMaxSize = false;
					}
					else if (wParam == SIZE_MAXIMIZED)
					{
						SetWindowPos(hWnd, NULL, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 0);
						mAppPaused = false;
						mMinimized = false;
						mMaximized = true;
						EngineGUI::mWindowMaxSize = true;
						OnResize(mClientWidth, mClientHeight);
					}
					else if (wParam == SIZE_RESTORED)
					{
						// Restoring from minimized state?
						if (mMinimized)
						{
							mAppPaused = false;
							mMinimized = false;
							OnResize(mClientWidth, mClientHeight);
						}
						// Restoring from maximized state?
						else if (mMaximized)
						{
							mAppPaused = false;
							mMaximized = false;
							EngineGUI::mWindowMaxSize = false;
							OnResize(mClientWidth, mClientHeight);
						}
						else if (mResizing)//正在调整大小
						{
							OnResize(mClientWidth, mClientHeight);
							//如果用户正在拖动调整大小条，我们不会在此处调整缓冲区的大小，
							//因为当用户不断拖动调整大小条时，会向窗口发送一个WM_SIZE消息流，
							//并且为每个WM_SIZE调整大小是没有意义的（并且很慢） 通过拖动调整大小条收到的消息。
							//因此，我们在用户完成窗口大小调整后释放调整大小条，然后发送WM_EXITSIZEMOVE消息。
						}
						else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
						{
							OnResize(mClientWidth, mClientHeight);
						}
					}
				}
				break;
			}

			case WM_ENTERSIZEMOVE:
				mAppPaused = true;
				mResizing = true;
				GameTimer::Stop();
				return 0;
			// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
	        // Here we reset everything based on the new window dimensions.
			case WM_EXITSIZEMOVE:
				mAppPaused = false;
				mResizing = false;
				GameTimer::Start();
				OnResize(mClientWidth, mClientHeight);
				break;
			case WM_LBUTTONDOWN:
			{
				OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				SystemParametersInfo(SPI_SETDRAGFULLWINDOWS, true, NULL, 0);
				//在客户区域实现拖动窗口
				if (GET_Y_LPARAM(lParam) < 22 && GET_X_LPARAM(lParam) > 80 &&
					GET_X_LPARAM(lParam) < (int)mClientWidth - 205)
				{
					ReleaseCapture();
					SendMessage(g_hWnd, WM_NCLBUTTONDOWN, HTCAPTION, NULL);
					SendMessage(g_hWnd, WM_LBUTTONUP, NULL, NULL);
				}
				//拖动改变窗口大小
				if (!mMaximized)
				{
					if (mCursorType != CursorType::Count)
					{
						SendMessage(g_hWnd, WM_NCLBUTTONDOWN, HTCAPTION, NULL);
						SendMessage(g_hWnd, WM_LBUTTONUP, NULL, NULL);
						switch (mCursorType)
						{
						case SIZENWSE:
							SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
							SendMessage(g_hWnd, WM_SYSCOMMAND, SC_SIZE | WMSZ_BOTTOMRIGHT, NULL);
							break;
						case SIZENS:
							SetCursor(LoadCursor(NULL, IDC_SIZENS));
							SendMessage(g_hWnd, WM_SYSCOMMAND, SC_SIZE | WMSZ_BOTTOM, NULL);
							break;
						case SIZEWE:
							SetCursor(LoadCursor(NULL, IDC_SIZEWE));
							SendMessage(g_hWnd, WM_SYSCOMMAND, SC_SIZE | WMSZ_RIGHT, NULL);
							break;
						case Count:
							break;
						default:
							break;
						}
					}
				}
				break;
			}
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
			// 抓住此消息以防止窗口变得太小。
			case WM_GETMINMAXINFO:
				((MINMAXINFO*)lParam)->ptMinTrackSize.x = 800;
				((MINMAXINFO*)lParam)->ptMinTrackSize.y = 450;
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