////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __APP_ANDROIDDISPLAY_H__
#define __APP_ANDROIDDISPLAY_H__


#include <NsCore/Noesis.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsApp/Display.h>


struct android_app;
struct AInputEvent;


namespace NoesisApp
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Implementation of Display using Android NDK.
////////////////////////////////////////////////////////////////////////////////////////////////////
class AndroidDisplay: public Display
{
public:
    AndroidDisplay();
    ~AndroidDisplay();

    /// From Display
    //@{
    void Show() override;
    void EnterMessageLoop(bool runInBackground) override;
    void Close() override;
    void OpenSoftwareKeyboard(Noesis::UIElement* focused) override;
    void CloseSoftwareKeyboard() override;
    void* GetNativeHandle() const override;
    uint32_t GetClientWidth() const override;
    uint32_t GetClientHeight() const override;
    //@}

private:
    bool IsReadyToRender() const; 
    bool ProcessMessages();

    static void OnAppCmd(android_app* app, int cmd);
    void DispatchAppEvent(int eventId);

    static int OnInputEvent(android_app* app, AInputEvent* event);
    int DispatchInputEvent(AInputEvent* event);

    void FillKeyTable();

private:
    android_app* mApp;

    int32_t mWidth, mHeight;

    bool mHasFocus;
    bool mIsVisible;
    bool mHasWindow;

    uint8_t mKeyTable[256];

    NS_DECLARE_REFLECTION(AndroidDisplay, Display)
};

}


#endif
