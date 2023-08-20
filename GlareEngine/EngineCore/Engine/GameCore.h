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

			//�˹��ܿ����ڳ�ʼ��Ӧ�ó���״̬�������ڷ����Ҫ��Ӳ����Դ�����С� 
			//����������Щ��Դ��ĳЩ״̬��Ӧ�ڹ��캯���г�ʼ��������ָ��ͱ�־��
			virtual void Startup(void) = 0;
			virtual void Cleanup(void) = 0;

			//ȷ�����Ƿ�Ҫ�˳���Ӧ�á� Ĭ������£�Ӧ�ó���������У�ֱ�����¡� ESC����Ϊֹ��
			virtual bool IsDone(void);

			//ÿ֡������һ��update������ ״̬���ºͳ�����Ⱦ��Ӧʹ�ô˷�������
			virtual void Update(float DeltaTime) = 0;

			//rendering pass
			virtual void RenderScene(void) = 0;

			//��ѡ��UI�����ǣ���Ⱦ�׶Ρ� ����LDR�� �������ѱ������ 
			virtual void RenderUI() = 0;

			//Resize
			virtual void OnResize(uint32_t width, uint32_t height) = 0;

			//Msg 
			virtual LRESULT  MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

			static GameApp* GetApp() { return mGameApp; }

			float AspectRatio() const;
		protected:
			// ���������������غ���
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


