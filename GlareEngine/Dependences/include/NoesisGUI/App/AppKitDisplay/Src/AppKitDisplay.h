////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __APP_APPKITDISPLAY_H__
#define __APP_APPKITDISPLAY_H__


#include <NsCore/Noesis.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsGui/Enums.h>
#include <NsApp/Display.h>
#include <CoreGraphics/CGBase.h>
#include <Foundation/NSString.h>
#include <AppKit/NSCursor.h>


@class WindowDelegate;
@class TextInputClient;


namespace NoesisApp
{

////////////////////////////////////////////////////////////////////////////////////////////////////
class AppKitDisplay final: public Display
{
public:
    AppKitDisplay();
    ~AppKitDisplay();

    /// From Display
    //@{
    void SetTitle(const char* title) override;
    void SetLocation(int x, int y) override;
    void SetSize(uint32_t width, uint32_t height) override;
    void Show() override;
    void EnterMessageLoop(bool runInBackground) override;
    void Close() override;
    void OpenSoftwareKeyboard(Noesis::UIElement* focused) override;
    void CloseSoftwareKeyboard() override;
    void SetCursor(Noesis::Cursor cursor) override;
    void OpenUrl(const char* url) override;
    void* GetNativeHandle() const override;
    uint32_t GetClientWidth() const override;
    uint32_t GetClientHeight() const override;
    //@}

    void FillKeyTable();
    void LoadCursors();

    void OnActivated();
    void OnDeactivated();
    void OnRender();
    void OnSizeChanged(CGFloat width, CGFloat height);
    void OnLocationChanged(CGFloat x, CGFloat y);
    void OnKeyDown(unsigned short key);
    void OnKeyUp(unsigned short key);
    void OnChar(unichar ch);
    void OnMouseDoubleClick(CGFloat x, CGFloat y, Noesis::MouseButton button);
    void OnMouseButtonDown(CGFloat x, CGFloat y, Noesis::MouseButton button);
    void OnMouseButtonUp(CGFloat x, CGFloat y, Noesis::MouseButton button);
    void OnMouseMove(CGFloat x, CGFloat y);
    void OnMouseWheel(CGFloat x, CGFloat y, CGFloat delta);
    void OnFileDropped(const char* filename);

public:
    bool mClosed;
    uint8_t mKeyTable[256];
    WindowDelegate* mWindow;
    TextInputClient* mTextInputClient;
    NSCursor* mCursors[Noesis::Cursor_Count];

    NS_DECLARE_REFLECTION(AppKitDisplay, Display)
};

}

#endif
