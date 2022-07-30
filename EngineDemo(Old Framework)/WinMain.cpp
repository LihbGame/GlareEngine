#include <Windows.h>
#include <windowsx.h>
#include "resource.h"

#include "Direct3D12Window.h"

HWND      hWnd = nullptr; // main window handle
HINSTANCE hInst = nullptr;
int i_Width = 1237, i_Height = 720;
std::wstring mMainWndCaption = L"Glare Engine";

bool      mAppPaused = false;  // is the application paused?
bool      mMinimized = false;  // is the application minimized?
bool      mMaximized = false;  // is the application maximized?
bool      mResizing = false;   // 是否被拖动调整大小的栏？
bool      mFullscreenState = false;// fullscreen enabled

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 此代码模块中包含的函数的前向声明:
ATOM				MyRegisterClass(HINSTANCE hInstance);
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

Direct3D12Window theApp;

//[System::STAThreadAttribute]
int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	hInst = hInstance;

	if (!MyRegisterClass(hInstance))
	{
		MessageBox(hWnd, L"注册窗口类失败！", L"错误！", MB_OK);
		return false;
	}

	hWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(), WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, i_Width, i_Height, 0, 0, hInstance, hPrevInstance);
	if (!hWnd)
	{
		MessageBox(hWnd, L"创建窗口失败！", L"错误！", MB_OK);
		return false;
	}

	theApp.SethWnd(hWnd);
	theApp.SetWinSize(i_Width, i_Height);

	if (!theApp.Initialize())
		return 0;

	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	MSG msg;

	theApp.mTimer.Reset();
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		// If there are Window messages then process them.
		if (!TranslateAccelerator(msg.hwnd, nullptr, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (!mAppPaused)
			{
				theApp.Update();
				theApp.Render();
				theApp.CalculateFrameStats();
			}
		}
	}

	return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = MainWndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wcex.hCursor = LoadCursor(hInstance, MAKEINTRESOURCE(IDC_CURSOR1));
	wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"MainWnd";
	wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	if (!RegisterClassExW(&wcex))
		return false;

	return true;
}

LRESULT MainWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam);


	switch (message)
	{
		// 激活或取消激活窗口时发送WM_ACTIVATE。
		//我们在停用窗口时暂停游戏，并在窗口激活时取消暂停。
	case WM_CREATE:
	{
		SetCursor(LoadCursor(hInst, MAKEINTRESOURCE(IDC_CURSOR1)));
	}
	break;
	
	case WM_ACTIVATE:
	{
		if (LOWORD(wParam) == WA_INACTIVE)//窗口没有激活
		{
			mAppPaused = true;
			theApp.mTimer.Stop();
		}
		else								//窗口激活
		{
			mAppPaused = false;
			theApp.mTimer.Start();
		}
	}
	break;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: 在此处添加使用 hdc 的任何绘图代码...
		EndPaint(hWnd, &ps);
	}
	break;

	// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
	{	
		// Save the new client area dimensions.
		i_Width = LOWORD(lParam);
		i_Height = HIWORD(lParam);
		if (i_Width * i_Height > 0)
		{
			theApp.SetWinSize(i_Width, i_Height);
			if (theApp.IsCreate())
			{
				if (wParam == SIZE_MINIMIZED)
				{
					mAppPaused = true;
					mMinimized = true;
					mMaximized = false;
				}
				else if (wParam == SIZE_MAXIMIZED)
				{
					mAppPaused = false;
					mMinimized = false;
					mMaximized = true;
					theApp.OnResize();
				}
				else if (wParam == SIZE_RESTORED)
				{

					// Restoring from minimized state?
					if (mMinimized)
					{
						mAppPaused = false;
						mMinimized = false;
						theApp.OnResize();
					}

					// Restoring from maximized state?
					else if (mMaximized)
					{
						mAppPaused = false;
						mMaximized = false;
						theApp.OnResize();
					}
					else if (mResizing)//正在调整大小
					{

						// 如果用户正在拖动调整大小条，我们不会在此处调整缓冲区的大小，
						//因为当用户不断拖动调整大小条时，会向窗口发送一个WM_SIZE消息流，
						//并且为每个WM_SIZE调整大小是没有意义的（并且很慢） 通过拖动调整大小条收到的消息。
						//因此，我们在用户完成窗口大小调整后释放调整大小条，然后发送WM_EXITSIZEMOVE消息。
					}
					else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
					{
						theApp.OnResize();
					}
				}
			}
		}
	}
	break;

	// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
	{
		mAppPaused = true;
		mResizing = true;
		theApp.mTimer.Stop();
	}
	break;

	// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
	// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
	{
		mAppPaused = false;
		mResizing = false;
		theApp.mTimer.Start();
		theApp.OnResize();
	}
	break;

	// 当菜单处于活动状态且用户按下与任何助记符或加速键不对应的键时，将发送WM_MENUCHAR消息。
	case WM_MENUCHAR:
	{
		// Don't beep when we alt-enter.
		return MAKELRESULT(0, MNC_CLOSE);
	}
	break;

	// 抓住此消息以防止窗口变得太小。
	case WM_GETMINMAXINFO:
	{
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 800;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 600;
	}
	break;

	case WM_LBUTTONDOWN:
	{
		theApp.OnLButtonDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		SystemParametersInfo(SPI_SETDRAGFULLWINDOWS, true, NULL, 0);

		//在客户区域实现拖动窗口
		if (GET_Y_LPARAM(lParam) < 22 &&
			GET_X_LPARAM(lParam) > 80 &&
			GET_X_LPARAM(lParam) < i_Width - 205
			&& !mFullscreenState)
		{
			ReleaseCapture();
			SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, NULL);
			SendMessage(hwnd, WM_LBUTTONUP, NULL, NULL);
		}
	}
	break;

	case WM_LBUTTONUP:
		theApp.OnLButtonUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	case WM_RBUTTONDOWN:
		theApp.OnRButtonDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	case WM_RBUTTONUP:
		theApp.OnLButtonUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	case WM_MBUTTONDOWN:
		theApp.OnMButtonDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	case WM_MBUTTONUP:
		theApp.OnMButtonUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	case WM_MOUSEMOVE:
		theApp.OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	case WM_MOUSEWHEEL:
		theApp.OnMouseWheel(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;

	case EVENT_SYSTEM_DESKTOPSWITCH:
		theApp.ActiveSystemDesktopSwap();
		break;

	case WM_KEYDOWN:
		theApp.OnKeyDown(static_cast<UINT8>(wParam));
		break;
	case WM_KEYUP:
		theApp.OnKeyUp(static_cast<UINT8>(wParam));
		break;

		// WM_DESTROY is sent when the window is being destroyed.
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}
