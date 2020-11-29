////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __APP_ENTRYPOINT_H__
#define __APP_ENTRYPOINT_H__


#include <NsCore/Noesis.h>
#include <windows.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
// Common main entry point for all Noesis applications
////////////////////////////////////////////////////////////////////////////////////////////////////
int NsMain(int argc, char** argv);
int NsMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR    lpCmdLine,
    _In_ int       nCmdShow);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Platform dependent application entry point
////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef NS_PLATFORM_WINDOWS_DESKTOP

#ifdef _CONSOLE
int main(int argc, char** argv)
{
    return NsMain(argc, argv);
}
#else
#include <windows.h>
#include <stdlib.h>

int APIENTRY WinMain(_In_ HINSTANCE in, _In_opt_ HINSTANCE ins, _In_ LPSTR str, _In_ int cmd)
{

    return NsMain(in, ins, str, cmd);
}
#endif



#elif defined(NS_PLATFORM_IPHONE)

#import <UIKit/UIKit.h>
#import <CoreFoundation/CFURL.h>

static int __argc;
static char** __argv;

@interface AppDelegate : NSObject<UIApplicationDelegate>
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(UIApplication*)application
{
    [[UIApplication sharedApplication]setStatusBarHidden:YES];
    [self performSelector : @selector(performInit:) withObject:nil afterDelay : 0.2f] ;
}

- (void)performInit:(id)object
{
    NsMain(__argc, __argv);
}

@end

int main(int argc, char** argv)
{
    __argc = argc;
    __argv = argv;

    // Set working directory to main bundle
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (bundle)
    {
        CFURLRef url = CFBundleCopyBundleURL(bundle);
        if (url)
        {
            char uri[PATH_MAX];
            if (CFURLGetFileSystemRepresentation(url, true, (UInt8*)uri, PATH_MAX))
            {
                chdir(uri);
            }

            CFRelease(url);
        }
    }

    return UIApplicationMain(argc, argv, nil, @"AppDelegate");
}

#elif defined(NS_PLATFORM_OSX)

int main(int argc, char** argv)
{
    return NsMain(argc, argv);
}

#elif defined(NS_PLATFORM_ANDROID)

#include <NsApp/Display.h>
#include <android_native_app_glue.h>

void android_main(android_app* app)
{
    // Store current app environment
    NoesisApp::Display::SetPrivateData(app);

    const char* argv[] = { "/system/bin/app_process" };
    NsMain(1, (char**)argv);

    // We need to kill the process because if we don't do that the native dynamic library will
    // be reused for the next activity and all our static variables will have garbage
    exit(0);
}

#elif defined(NS_PLATFORM_LINUX) || defined(NS_PLATFORM_EMSCRIPTEN)

int main(int argc, char** argv)
{
    return NsMain(argc, argv);
}

// <xbox_one>
#elif defined(NS_PLATFORM_XBOX_ONE)

#include <NsCore/UTF8.h>
#include <string.h>

using namespace Windows::Foundation;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::UI::Core;

ref class ViewProvider sealed : public IFrameworkView
{
public:
    virtual void Initialize(CoreApplicationView^ applicationView)
    {
        applicationView->Activated += ref new TypedEventHandler<CoreApplicationView^,
            IActivatedEventArgs^>(this, &ViewProvider::OnActivated);
    }

    virtual void Uninitialize() {}
    virtual void SetWindow(CoreWindow^ window) {}
    virtual void Load(Platform::String^ entryPoint) {}

    virtual void Run()
    {
        char args[_MAX_PATH];
        Noesis::UTF8::UTF16To8((const uint16_t*)args_->Data(), args, sizeof(args));

        int argc = 1;
        char* argv[16] = { "App.exe" };
        char* context;

        argv[argc++] = strtok_s(args, " ", &context);

        while (argv[argc - 1] != 0 && argc < _countof(argv))
        {
            argv[argc++] = strtok_s(nullptr, " ", &context);
        }

        NsMain(argc - 1, argv);
    }

private:
    void OnActivated(CoreApplicationView^ applicationView, IActivatedEventArgs^ args)
    {
        if (args->Kind == ActivationKind::Launch)
        {
            LaunchActivatedEventArgs^ launchArgs = (LaunchActivatedEventArgs^)args;
            args_ = launchArgs->Arguments;
        }

        CoreWindow::GetForCurrentThread()->Activate();
    }

    Platform::String^ args_;
};

ref class ViewProviderFactory : IFrameworkViewSource
{
public:
    virtual IFrameworkView^ CreateView()
    {
        return ref new ViewProvider();
    }
};

[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^ argv)
{
    CoreApplication::DisableKinectGpuReservation = true;
    auto viewProviderFactory = ref new ViewProviderFactory();
    CoreApplication::Run(viewProviderFactory);
    return 0;
}
// </xbox_one>

// <ps4>
#elif defined(NS_PLATFORM_PS4)

#include <stdlib.h>

// Switch to dynamic allocation
unsigned int sceLibcHeapExtendedAlloc = 1;

// No upper limit for heap area
size_t sceLibcHeapSize = SCE_LIBC_HEAP_SIZE_EXTENDED_ALLOC_NO_LIMIT;

int main(int argc, char** argv)
{
    return NsMain(argc, argv);
}
// </ps4>

// <nx>
#elif defined(NS_PLATFORM_NX)

#include <nn/os/os_Argument.h>

extern "C" void nnMain()
{
    NsMain(nn::os::GetHostArgc(), nn::os::GetHostArgv());
}
// </nx>

#else
#error Unknown platform
#endif


#endif
