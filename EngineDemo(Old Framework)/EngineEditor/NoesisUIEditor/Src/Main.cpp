////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <NsRender/RenderContext.h>
#include <NsCore/HighResTimer.h>
#include <NsGui/IntegrationAPI.h>
#include <NsGui/UserControl.h>
#include <NsGui/IRenderer.h>
#include <NsGui/IView.h>
#include <NsApp/EntryPoint.h>
#include <NsApp/Launcher.h>
#include <NsApp/Display.h>
#include <NsApp/EmbeddedXamlProvider.h>
#include <NsApp/EmbeddedFontProvider.h>

#include "d3dApp.h"					//����ͨ��Ӧ�ó���ͷ�ļ�;
#include "MathHelper.h"				//������ѧ����ͷ�ļ�;
#include "UploadBuffer.h"			//������Դ�ϴ�ͷ�ļ�;
#include "WindowsProject_Box.h"

// For this sample we are embedding needed resources using our tool 'binh'
#include "Settings.xaml.bin.h"
#include "HermeneusOne-Regular.ttf.bin.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
void InstallResourceProviders()
{
    // Each time a resource (xaml, texture, font) is needed, the corresponding provider is invoked
    // to get a stream to the content. You must install a provider for each needed resource. There
    // are a few implementations available in the app framework (like LocalXamlProvider to load
    // from disk). For this sample, we are embedding needed resources in a C array.
    NoesisApp::EmbeddedXaml xaml =
    { 
        "Settings.xaml", Settings_xaml, sizeof(Settings_xaml)
    };

    Noesis::GUI::SetXamlProvider(Noesis::MakePtr<NoesisApp::EmbeddedXamlProvider>(&xaml, 1));

    NoesisApp::EmbeddedFont font = 
    {
        "", HermeneusOne_Regular_ttf, sizeof(HermeneusOne_Regular_ttf)
    };

    Noesis::GUI::SetFontProvider(Noesis::MakePtr<NoesisApp::EmbeddedFontProvider>(&font, 1));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Noesis::Ptr<NoesisApp::RenderContext> CreateContext(NoesisApp::Display* display)
{
    // The render context initializes the graphic subsystem (D3D11, GL, Metal, ...). It also
    // provides a RenderDevice object that is needed to initialize each View. This is a helper
    // class, not part of core. The only mandatory part that you need is an implementation of the
    // RenderDevice base class. We provide reference implementation for many subsystems
    uint32_t samples = 1;
    Noesis::Ptr<NoesisApp::RenderContext> context = NoesisApp::RenderContext::Create();
    context->Init(display->GetNativeHandle(), samples, true, false);
    return context;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Noesis::Ptr<Noesis::IView> CreateView(Noesis::RenderDevice* device)
{
    Noesis::Ptr<Noesis::IView> view;

    // You need a view to render the user interface and interact with it. A view holds a tree of
    // elements. The easiest way to construct a tree is from a XAML file
    view = Noesis::GUI::CreateView(Noesis::GUI::LoadXaml<Noesis::UserControl>("Settings.xaml"));

    // As we are not using MSAA in this sample, we enable PPAA here. PPAA is a cheap antialising
    // technique that extrudes the contours of the geometry smoothing them
    view->SetIsPPAAEnabled(true);

    // Once the view is created you need to get its internal renderer and initialize it with a valid
    // render device. This can be done in a separate render thread but for simplification purposes
    // we are using the same thread in this sample
    view->GetRenderer()->Init(device);

    return view;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SetupDisplay(NoesisApp::Display* display, NoesisApp::RenderContext* ctx, Noesis::IView* view)
{
    // Gather window events and send to view
    display->SizeChanged() += [view, ctx](NoesisApp::Display* display, uint32_t, uint32_t)
    {
        view->SetSize(display->GetClientWidth(), display->GetClientHeight());
        ctx->Resize();
    };

    display->Activated() += [view](NoesisApp::Display*)
    {
        view->Activate();
    };

    display->Deactivated() += [view](NoesisApp::Display*)
    {
        view->Deactivate();
    };

    display->MouseButtonUp() += [view](NoesisApp::Display*, int x, int y, Noesis::MouseButton b)
    {
        view->MouseButtonUp(x, y, b);
    };

    display->MouseButtonDown() += [view](NoesisApp::Display*, int x, int y, Noesis::MouseButton b)
    {
        view->MouseButtonDown(x, y, b);
    };

    display->MouseMove() += [view](NoesisApp::Display*, int x, int y)
    {
        view->MouseMove(x, y);
    };

    display->MouseDoubleClick() += [view](NoesisApp::Display*, int x, int y, Noesis::MouseButton b)
    {
        view->MouseDoubleClick(x, y, b);
    };

    display->MouseWheel() += [view](NoesisApp::Display*, int x, int y, int delta)
    {
        view->MouseWheel(x, y, delta);
    };

    display->TouchUp() += [view](NoesisApp::Display*, int x, int y, uint64_t id)
    {
        view->TouchUp(x, y, id);
    };

    display->TouchDown() += [view](NoesisApp::Display*, int x, int y, uint64_t id)
    {
        view->TouchDown(x, y, id);
    };

    display->TouchMove() += [view](NoesisApp::Display*, int x, int y, uint64_t id)
    {
        view->TouchMove(x, y, id);
    };

    display->KeyDown() += [view](NoesisApp::Display*, Noesis::Key key)
    {
        view->KeyDown(key);
    };

    display->KeyUp() += [view](NoesisApp::Display*, Noesis::Key key)
    {
        view->KeyUp(key);
    };

    display->Char() += [view](NoesisApp::Display*, uint32_t c)
    {
        view->Char(c);
    };

    // And finally, make the window visible
    display->Show();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void RenderView(Noesis::IView* view, NoesisApp::RenderContext* ctx, uint32_t width, uint32_t height, GameTimer& timer, BoxApp& app,MSG& msg)
{
    // Begin rendering
    ctx->BeginRender();

   
    timer.Tick();
    app.Draw(timer);
    app.Update(timer);

    // The offscreen rendering phase must be done before binding the framebuffer. This step
    // populates all the internal textures that are needed for the active frame
    view->GetRenderer()->UpdateRenderTree();
    view->GetRenderer()->RenderOffscreen();

    // Bind framebuffer and viewport. Do this per frame because the offscreen phase alters them
    ctx->SetDefaultRenderTarget(width, height, true);

    // -> # At this point, you can render your 3D scene... # <-
    // Note that each function invocation of the renderer modifies the GPU state, you must
    // save and restore it properly in your application

    // After rendering the scene we draw the UI on top of it. Note that the render is performed
    // in the active framebuffer, being it the backbuffer or a render texture
    view->GetRenderer()->Render();
   
    // End render and swap back buffer
    ctx->EndRender();
    ctx->Swap();

}

////////////////////////////////////////////////////////////////////////////////////////////////////
int NsMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR    lpCmdLine,
	_In_ int       nCmdShow)
{


    auto logHandler = [](const char*, uint32_t, uint32_t level, const char*, const char* message)
    {
        // [TRACE] [DEBUG] [INFO] [WARNING] [ERROR]
        const char* prefixes[] = { "T", "D", "I", "W", "E" };
        printf("[NOESIS/%s] %s\n", prefixes[level], message);
    };

    // Noesis initialization. This must be the first step before using any NoesisGUI functionality.
    // A logging handler is installed here. You can also install a custom error handler and memory
    // allocator. By default errors are redirected to the logging handler
    Noesis::GUI::Init(nullptr, logHandler, nullptr);

    // Register app components. We need a few in this example, like Display and RenderContext
    NoesisApp::Launcher::RegisterAppComponents();

    // Setup providers for each resource
    InstallResourceProviders();

    // A display is an abstraction over a system window (HWND, NSWindow, UIWindow, XWindow, ...).
    // As this is a helper class, not part of core, it goes in the 'NoesisApp' namespace
    Noesis::Ptr<NoesisApp::Display> display = NoesisApp::CreateDisplay();
    display->SetTitle("NoesisGUI Integration Sample");

    // Render context creation
    Noesis::Ptr<NoesisApp::RenderContext> context = CreateContext(display);

    // Create view passing the render device implementation created by the context
    Noesis::Ptr<Noesis::IView> view = CreateView(context->GetDevice());

    // Setup input event redirection from window to view
    SetupDisplay(display, context, view);


	static BoxApp theApp(hInstance);
	if (!theApp.Initialize((HWND)display->GetNativeHandle()))
		return 0;

   static GameTimer timer;
    timer.Reset();

    static MSG msg = { 0 };
 /*   WNDPROC  OldProc = (WNDPROC)SetWindowLong(theApp.mhParentWnd, GWL_WNDPROC, (LONG)MainWndProc);
    theApp.GetUIWndProc(OldProc);*/
    // Main loop
    display->Render() += [&view, &context](NoesisApp::Display* display)
    {
        // If there are Window messages then process them.
        static uint64_t startTime = Noesis::HighResTimer::Ticks();
        // Update (layout, animations). Note that global time is used, not a delta
        uint64_t time = Noesis::HighResTimer::Ticks();
        view->Update(Noesis::HighResTimer::Seconds(time - startTime));
        // Render (this could be done in a separate render thread)
        RenderView(view, context, display->GetClientWidth(), display->GetClientHeight(), timer, theApp,msg);
    };

    display->EnterMessageLoop(false);

    // Close view. This step must be done in the same thread where initialization was performed
    view->GetRenderer()->Shutdown();

    // It is mandatory to release all noesis objects before shutting down
    view.Reset();
    context.Reset();
    display.Reset();

    // And that's all, thanks for your services noesis :)
    Noesis::GUI::Shutdown();

    return 0;
}
