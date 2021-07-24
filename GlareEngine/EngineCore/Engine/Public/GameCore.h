#pragma once

namespace GlareEngine
{
	namespace GameCore
	{

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
			virtual void Update(float deltaT) = 0;

			//rendering pass
			virtual void RenderScene(void) = 0;

			//可选的UI（覆盖）渲染阶段。 这是LDR。 缓冲区已被清除。 
			virtual void RenderUI() = 0;

			//Resize
			virtual void OnResize(uint32_t width, uint32_t height) = 0;

			//Msg 
			virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

			static GameApp* GetApp() { return mGameApp; }

			float AspectRatio() const;
		
		protected:
			static GameApp* mGameApp;

			uint32_t mClientWidth;
			uint32_t mClientHeight;
		};

		void RunApplication(GameApp& app, const wchar_t* className,HINSTANCE HAND);


	}
	

#define MAIN_FUNCTION()  int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,\
	PSTR cmdLine, int showCmd)
#define CREATE_APPLICATION( app_class ) \
    MAIN_FUNCTION() \
    { \
        GameApp* app = new app_class(); \
        GlareEngine::GameCore::RunApplication( *app, L#app_class,hInstance); \
        delete app; \
        return 0; \
    }

}


