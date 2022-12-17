#include "Graphics/CommandContext.h"
#include "Graphics/GraphicsCore.h"
#include "GameCore.h"
#include "GameTimer.h"
#include "EngineInput.h"
#include "Graphics/BufferManager.h"
#include "EngineAdjust.h"
#include "EngineProfiling.h"
#include "EngineGUI.h"
#include "PostProcessing/PostProcessing.h"

#include <dwmapi.h>
#include <windowsx.h>
#include "resource.h"

#define CURSOR_NAME "../Resource/Textures/ICONS/Anime Cursor/AnimeCursor2.ani"
#define CURSOR_SIZENWSE "../Resource/Textures/ICONS/Cursor/dgn1.cur"
#define CURSOR_SIZENS "../Resource/Textures/ICONS/Cursor/vert.cur"
#define CURSOR_SIZEWE "../Resource/Textures/ICONS/Cursor/horz.cur"

#pragma comment(lib, "runtimeobject.lib")
#pragma comment(lib, "Dwmapi.lib")
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
		using namespace GlareEngine::Display;

		GameApp* GameApp::mGameApp = nullptr;
		HWND g_hWnd = nullptr;
		HCURSOR gCursor = nullptr;
		HCURSOR gCursorSizeNWSE = nullptr;
		HCURSOR gCursorSizeNS = nullptr;
		HCURSOR gCursorSizeWE = nullptr;


		LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			//转发hwnd因为我们可以在CreateWindow返回之前获取消息(例如，WM_CREATE)。
			return GameApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
		}

		void InitializeWindow(HINSTANCE hand, const wchar_t* className)
		{
			//当前所调用该函数的程序实例句柄
			HINSTANCE hInst = hand;

			gCursor = LoadCursorFromFileA(CURSOR_NAME);
			gCursorSizeNWSE = LoadCursorFromFileA(CURSOR_SIZENWSE);
			gCursorSizeNS = LoadCursorFromFileA(CURSOR_SIZENS);
			gCursorSizeWE = LoadCursorFromFileA(CURSOR_SIZEWE);

			// Register class
			WNDCLASSEX wcex;
			wcex.cbSize = sizeof(WNDCLASSEX);
			wcex.style = CS_HREDRAW | CS_VREDRAW;
			wcex.lpfnWndProc = MainWndProc;
			wcex.cbClsExtra = 0;
			wcex.cbWndExtra = 0;
			wcex.hInstance = hInst;
			wcex.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON5));
			wcex.hCursor = gCursor;// LoadCursor(hInst, MAKEINTRESOURCE(IDC_CURSOR1));
			wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
			wcex.lpszMenuName = nullptr;
			wcex.lpszClassName = className;
			wcex.hIconSm = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON6));
			if (!RegisterClassEx(&wcex))
			{
				DWORD dwerr = GetLastError();
				MessageBox(0, L"RegisterClass Failed.", 0, 0);
				return;
			}
			// Create window
			RECT rc = { 0, 0,(LONG)g_DisplayWidth, (LONG)g_DisplayHeight };
			AdjustWindowRect(&rc, WS_THICKFRAME | WS_MAXIMIZEBOX | WS_POPUPWINDOW, false);


			g_hWnd = CreateWindow(className, className, WS_THICKFRAME | WS_MAXIMIZEBOX | WS_POPUPWINDOW, 100, 100,
				rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);

			assert(g_hWnd != 0);
		}

		void PreRender(GameApp& Game)
		{
			//Tone Mapping hdr or ldr
			Display::PreparePresent();
			//RenderUI
			Game.RenderUI();
			//Present
			Display::Present();
		}

		void InitializeApplication(GameApp& Game)
		{
			//Core Initialize
			InitializeGraphics();
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
			TextureManager::Shutdown();
		}

		bool UpdateApplication(GameApp& Game)
		{
			GameTimer::Tick();
			float DeltaTime = Display::GetFrameTime();

			EngineProfiling::Update();
			EngineInput::Update(DeltaTime);
			EngineAdjust::Update(DeltaTime);

			//Update Game
			Game.Update(DeltaTime);
			//RenderScene
			Game.RenderScene();

			//PostProcessing::DrawBeforeToneMapping();

			//Tone Mapping hdr or ldr
			Display::PreparePresent();
			//RenderUI
			Game.RenderUI();

			//Present
			Display::Present();

			return !Game.IsDone();
		}


		GameApp::GameApp()
		{
			// Only one GameApp can be constructed.
			assert(mGameApp == nullptr);
			mGameApp = this;
			//init MonitorInfo
			mMonitorInfo.cbSize = sizeof(MONITORINFO);
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

			Microsoft::WRL::Wrappers::RoInitializeWrapper InitializeWinRT(RO_INIT_MULTITHREADED);
			ThrowIfFailed(InitializeWinRT);

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

			ShutdownGraphics();
			TerminateApplication(app);
			
		}


		//Pretest  for windows 11
		enum DWMWINDOWATTRIBUTE
		{
			DWMWA_WINDOW_CORNER_PREFERENCE = 33
		};
		
		enum DWM_WINDOW_CORNER_PREFERENCE
		{
			DWMWCP_DEFAULT = 0,
			DWMWCP_DONOTROUND = 1,
			DWMWCP_ROUND = 2,
			DWMWCP_ROUNDSMALL = 3
		};


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

			//case WM_NCHITTEST:
			//{
			//	if (hWnd)
			//	{
			//		// Get the point in screen coordinates.
			//		// GET_X_LPARAM and GET_Y_LPARAM are defined in windowsx.h
			//		POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			//		// Map the point to client coordinates.
			//		::MapWindowPoints(nullptr, hWnd, &point, 1);
			//		// If the point is in your maximize button then return HTMAXBUTTON
			//		if (::PtInRect(&m_maximizeButtonRect, point))
			//		{
			//			return HTMAXBUTTON;
			//		}
			//	}
			//	break;
			//}

			case WM_CREATE:
			{
				//Apply rounded corners in desktop apps for Windows 11
				if (hWnd)
				{
					DWORD preference = DWMWCP_ROUND;
					DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
				}
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
						//monitor full screen
						//SetWindowPos(hWnd, NULL, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 0);
						//SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);

						//Client area full screen
						RECT rect;
						DWORD flag = 0;
						GetMonitorInfo(MonitorFromWindow(hWnd, flag), &mMonitorInfo);
						rect = mMonitorInfo.rcWork;
						SetWindowPos(hWnd, NULL, rect.left, rect.top, rect.right- rect.left, rect.bottom- rect.top, 0);

						mAppPaused = false;
						mMinimized = false;
						mMaximized = true;
						EngineGUI::mWindowMaxSize = true;
						OnResize(mClientWidth, mClientHeight);
					}
					else if (wParam == SIZE_RESTORED)
					{
						OnResize(mClientWidth, mClientHeight);
						UpdateApplication(*GameApp::GetApp());

						// Restoring from minimized state?
						if (mMinimized)
						{
							mAppPaused = false;
							mMinimized = false;
						}
						// Restoring from maximized state?
						else if (mMaximized)
						{
							mAppPaused = false;
							mMaximized = false;
							EngineGUI::mWindowMaxSize = false;
						}
						else if (mResizing)//正在调整大小
						{
						}
						else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
						{
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
				break;
			case WM_LBUTTONDOWN:
			{
				OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				SystemParametersInfo(SPI_SETDRAGFULLWINDOWS, true, NULL, 0);
				//在客户区域实现拖动窗口
				if (GET_Y_LPARAM(lParam) < 22 && GET_X_LPARAM(lParam) > 80 &&
					GET_X_LPARAM(lParam) < (int)mClientWidth - 130)
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
						case CursorType::SIZENWSE:
							SetCursor(gCursorSizeNWSE);
							SendMessage(g_hWnd, WM_SYSCOMMAND, SC_SIZE | WMSZ_BOTTOMRIGHT, NULL);
							break;
						case CursorType::SIZENS:
							SetCursor(gCursorSizeNS);
							SendMessage(g_hWnd, WM_SYSCOMMAND, SC_SIZE | WMSZ_BOTTOM, NULL);
							break;
						case CursorType::SIZEWE:
							SetCursor(gCursorSizeWE);
							SendMessage(g_hWnd, WM_SYSCOMMAND, SC_SIZE | WMSZ_RIGHT, NULL);
							break;
						case CursorType::Count:
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
			case WM_KEYUP:
				if (wParam == VK_F1)
				{
					mIsHideUI = !mIsHideUI;
				}
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