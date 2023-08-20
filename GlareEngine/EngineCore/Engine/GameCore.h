#pragma once
#include "EngineThread.h"
#include "EngineConfig.h"
#define RESIZE_RANGE 3
enum class CursorType
{
	SIZENWSE,
	SIZENS,
	SIZEWE,
	Count
};

namespace GlareEngine
{
	namespace GameCore
	{
		extern HWND g_hWnd;
		class GameApp
		{
		public:
			GameApp();

			virtual ~GameApp() {}

			//此功能可用于初始化应用程序状态，并将在分配必要的硬件资源后运行。 
			//不依赖于这些资源的某些状态仍应在构造函数中初始化，例如指针和标志。
			virtual void Startup(void) = 0;
			virtual void Cleanup(void) = 0;

			//确定您是否要退出该应用。 默认情况下，应用程序继续运行，直到按下“ ESC”键为止。
			virtual bool IsDone(void);

			//每帧将调用一次update方法。 状态更新和场景渲染都应使用此方法处理。
			virtual void Update(float DeltaTime) = 0;

			//rendering pass
			virtual void RenderScene(void) = 0;

			//可选的UI（覆盖）渲染阶段。 这是LDR。 缓冲区已被清除。 
			virtual void RenderUI() = 0;

			//Resize
			virtual void OnResize(uint32_t width, uint32_t height) = 0;

			//Msg 
			virtual LRESULT  MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

			static GameApp* GetApp() { return mGameApp; }

			float AspectRatio() const;
		protected:
			// 处理鼠标输入的重载函数
			virtual void OnMouseDown(WPARAM btnState, int x, int y) {}
			virtual void OnMouseUp(WPARAM btnState, int x, int y) {}
			virtual void OnMouseMove(WPARAM btnState, int x, int y) {}

		protected:
			static GameApp* mGameApp;

			bool      mAppPaused = false;  // is the application paused?
			bool      mMinimized = false;  // is the application minimized?
			bool      mMaximized = false;  // is the application maximized?
			bool      mResizing = false;
			bool	  mIsHideUI = false;
		public:
			uint32_t mClientWidth = DEAFAULT_DISPLAY_SIZE_X;
			uint32_t mClientHeight = DEAFAULT_DISPLAY_SIZE_Y;

			MONITORINFO mMonitorInfo;
			POINT mLastMousePos;
			CursorType mCursorType = CursorType::Count;
		};

		void RunApplication(GameApp& app, const wchar_t* className, HINSTANCE HAND);

		extern HCURSOR gCursor;
		extern HCURSOR gCursorSizeNWSE;
		extern HCURSOR gCursorSizeNS;
		extern HCURSOR gCursorSizeWE;
	}


#define CREATE_APPLICATION( app_class ) \
    int APIENTRY wWinMain(_In_ HINSTANCE hInstance,_In_opt_ HINSTANCE hPrevInstance,_In_  LPWSTR    lpCmdLine,_In_  int nCmdShow) \
    { \
try\
	{\
        GameApp* app = new app_class(); \
        GlareEngine::GameCore::RunApplication( *app, L#app_class,hInstance); \
        delete app; \
	}\
catch (DxException& e)\
	{\
		MessageBox(nullptr, e.ToString().c_str(), L"DX12 HR Failed", MB_OK);\
	}\
        return 0; \
    }

}


