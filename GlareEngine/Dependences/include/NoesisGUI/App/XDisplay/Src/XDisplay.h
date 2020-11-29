////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __APP_XDISPLAY_H__
#define __APP_XDISPLAY_H__


#include <NsCore/Noesis.h>
#include <NsApp/Display.h>
#include <NsCore/ReflectionDeclare.h>

#include <X11/Xlib.h>


namespace NoesisApp
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// X Window System display implementation
////////////////////////////////////////////////////////////////////////////////////////////////////
class XDisplay final: public Display
{
public:
    XDisplay();
    ~XDisplay();

    /// From Display
    //@{
    void SetTitle(const char* title) override;
    void SetLocation(int x, int y) override;
    void SetSize(uint32_t width, uint32_t height) override;
    void Show() override;
    void EnterMessageLoop(bool runInBackground) override;
    void Close() override;
    void* GetNativeHandle() const override;
    uint32_t GetClientWidth() const override;
    uint32_t GetClientHeight() const override;
    //@}

private:
    struct Handle
    {
        Window window;
        ::Display *display;
    };

    Handle _x;
    XIM _xim;
    XIC _xic;

    NS_DECLARE_REFLECTION(XDisplay, Display)
};

}

#endif
