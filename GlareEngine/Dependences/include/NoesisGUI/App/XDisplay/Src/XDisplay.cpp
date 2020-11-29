////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


// #undef NS_MINIMUM_LOG_LEVEL
// #define NS_MINIMUM_LOG_LEVEL 0


#include "XDisplay.h"

#include <NsCore/TypeId.h>
#include <NsCore/Log.h>
#include <NsCore/Category.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/UTF8.h>
#include <NsGui/InputEnums.h>

#include <X11/Xutil.h>
#include <locale.h>


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
static Key NoesisKey(KeySym k)
{
    switch (k)
    {
        case XK_Cancel: return Key_Cancel;
        case XK_BackSpace: return Key_Back;
        case XK_Tab: return Key_Tab;
        case XK_Linefeed: return Key_LineFeed;
        case XK_Clear: return Key_Clear;
        case XK_Return: return Key_Return;
        case XK_Pause: return Key_Pause;
        case XK_Caps_Lock: return Key_CapsLock;
        case XK_Escape: return Key_Escape;
        case XK_space: return Key_Space;
        case XK_Page_Up: return Key_PageUp;
        case XK_Page_Down: return Key_PageDown;
        case XK_End: return Key_End;
        case XK_Home: return Key_Home;
        case XK_Left: return Key_Left;
        case XK_Up: return Key_Up;
        case XK_Right: return Key_Right;
        case XK_Down: return Key_Down;
        case XK_Select: return Key_Select;
        case XK_Execute: return Key_Execute;
        case XK_Print: return Key_PrintScreen;
        case XK_Insert: return Key_Insert;
        case XK_Delete: return Key_Delete;
        case XK_Help: return Key_Help;
        case XK_0: return Key_D0;
        case XK_1: return Key_D1;
        case XK_2: return Key_D2;
        case XK_3: return Key_D3;
        case XK_4: return Key_D4;
        case XK_5: return Key_D5;
        case XK_6: return Key_D6;
        case XK_7: return Key_D7;
        case XK_8: return Key_D8;
        case XK_9: return Key_D9;
        case XK_a: return Key_A;
        case XK_b: return Key_B;
        case XK_c: return Key_C;
        case XK_d: return Key_D;
        case XK_e: return Key_E;
        case XK_f: return Key_F;
        case XK_g: return Key_G;
        case XK_h: return Key_H;
        case XK_i: return Key_I;
        case XK_j: return Key_J;
        case XK_k: return Key_K;
        case XK_l: return Key_L;
        case XK_m: return Key_M;
        case XK_n: return Key_N;
        case XK_o: return Key_O;
        case XK_p: return Key_P;
        case XK_q: return Key_Q;
        case XK_r: return Key_R;
        case XK_s: return Key_S;
        case XK_t: return Key_T;
        case XK_u: return Key_U;
        case XK_v: return Key_V;
        case XK_w: return Key_W;
        case XK_x: return Key_X;
        case XK_y: return Key_Y;
        case XK_z: return Key_Z;
        case XK_KP_0: return Key_NumPad0;
        case XK_KP_1: return Key_NumPad1;
        case XK_KP_2: return Key_NumPad2;
        case XK_KP_3: return Key_NumPad3;
        case XK_KP_4: return Key_NumPad4;
        case XK_KP_5: return Key_NumPad5;
        case XK_KP_6: return Key_NumPad6;
        case XK_KP_7: return Key_NumPad7;
        case XK_KP_8: return Key_NumPad8;
        case XK_KP_9: return Key_NumPad9;
        case XK_KP_Multiply: return Key_Multiply;
        case XK_KP_Add: return Key_Add;
        case XK_KP_Separator: return Key_Separator;
        case XK_KP_Subtract: return Key_Subtract;
        case XK_KP_Decimal: return Key_Decimal;
        case XK_KP_Divide: return Key_Divide;
        case XK_F1: return Key_F1;
        case XK_F2: return Key_F2;
        case XK_F3: return Key_F3;
        case XK_F4: return Key_F4;
        case XK_F5: return Key_F5;
        case XK_F6: return Key_F6;
        case XK_F7: return Key_F7;
        case XK_F8: return Key_F8;
        case XK_F9: return Key_F9;
        case XK_F10: return Key_F10;
        case XK_F11: return Key_F11;
        case XK_F12: return Key_F12;
        case XK_F13: return Key_F13;
        case XK_F14: return Key_F14;
        case XK_F15: return Key_F15;
        case XK_F16: return Key_F16;
        case XK_F17: return Key_F17;
        case XK_F18: return Key_F18;
        case XK_F19: return Key_F19;
        case XK_F20: return Key_F20;
        case XK_F21: return Key_F21;
        case XK_F22: return Key_F22;
        case XK_F23: return Key_F23;
        case XK_F24: return Key_F24;
        case XK_Num_Lock: return Key_NumLock;
        case XK_Scroll_Lock: return Key_Scroll;
        case XK_Shift_L: return Key_LeftShift;
        case XK_Shift_R: return Key_RightShift;
        case XK_Control_L: return Key_LeftCtrl;
        case XK_Control_R: return Key_RightCtrl;
        case XK_Alt_L: return Key_LeftAlt;
        case XK_Alt_R: return Key_RightAlt;
    }

    return Key_None;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
XDisplay::XDisplay()
{
    // Open display
    _x.display = XOpenDisplay(0);
    if (_x.display == 0)
    {
        NS_FATAL("Failed to open X display");
    }
    
    int screenNumber = DefaultScreen(_x.display);
    int depth = DefaultDepth(_x.display, screenNumber);
    ::Visual* visual = DefaultVisual(_x.display, screenNumber);

    // Create a color map
    Window rootWindow = RootWindow(_x.display, screenNumber);
    Colormap colorMap = XCreateColormap(_x.display, rootWindow, visual, AllocNone);

    // Create window
    XSetWindowAttributes swa;
    swa.colormap = colorMap;
    swa.border_pixel = 0;
    swa.event_mask = FocusChangeMask | StructureNotifyMask | SubstructureNotifyMask
        | PointerMotionMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask;

    _x.window = XCreateWindow(_x.display, rootWindow, 0, 0, 1, 1, 0, depth, InputOutput, visual,
        CWBorderPixel | CWColormap | CWEventMask, &swa);
    NS_ASSERT(_x.window != 0);

    // Resize window to nice default dimensions
    Screen* screen = XScreenOfDisplay(_x.display, screenNumber);
    int screenWidth = XWidthOfScreen(screen);
    int screenHeight = XHeightOfScreen(screen);
    uint32_t width = 74 * screenWidth / 100;
    uint32_t height = width * screenHeight / screenWidth;
    XResizeWindow(_x.display, _x.window, width, height);

    setlocale(LC_CTYPE, "");
    XSetLocaleModifiers("");

    _xim = XOpenIM(_x.display, 0, 0, 0);
    if (_xim == 0)
    {
        XSetLocaleModifiers("@im=none");
        _xim = XOpenIM(_x.display, 0, 0, 0);
    }

    if (_xim != 0)
    {
        _xic = XCreateIC(_xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow,
            _x.window, XNFocusWindow, _x.window, NULL);

        if (_xic != 0)
        {
            XSetICFocus(_xic);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
XDisplay::~XDisplay()
{
    if (_xic != 0)
    {
        XDestroyIC(_xic);
    }

    if (_xim != 0)
    {
        XCloseIM(_xim);
    }

    XWindowAttributes attrs;
    XGetWindowAttributes(_x.display, _x.window, &attrs);

    XFreeColormap(_x.display, attrs.colormap);
    XDestroyWindow(_x.display, _x.window);

    XCloseDisplay(_x.display);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void XDisplay::SetTitle(const char* title)
{
    XStoreName(_x.display, _x.window, title);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void XDisplay::SetLocation(int x, int y)
{
    XMoveWindow(_x.display, _x.window, x, y);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void XDisplay::SetSize(uint32_t width, uint32_t height)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(_x.display, _x.window, &attrs);
    XResizeWindow(_x.display, _x.window, width + attrs.border_width, height + attrs.border_width);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void XDisplay::Show()
{
    XMapWindow(_x.display, _x.window);
    XRaiseWindow(_x.display, _x.window);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void XDisplay::EnterMessageLoop(bool /*runInBackground*/)
{
    for (;;)
    {
        while (XPending(_x.display))
        {
            XEvent event;
            XNextEvent(_x.display, &event);

            if (XFilterEvent(&event, None))
            {
                continue;
            }

            switch (event.type)
            {
                case ConfigureNotify:
                {
                    XConfigureEvent* e = (XConfigureEvent*)&event;
                    mLocationChanged(this, e->x, e->y);
                    mSizeChanged(this, e->width, e->height);
                    break;
                }
                case FocusIn:
                {
                    mActivated(this);
                    break;
                }
                case FocusOut:
                {
                    mDeactivated(this);
                    break;
                }
                case MotionNotify:
                {
                    XMotionEvent* e = (XMotionEvent*)&event;
                    mMouseMove(this, e->x, e->y);
                    break;
                }
                case ButtonPress:
                {
                    XButtonEvent* e = (XButtonEvent*)&event;

                    switch (e->button)
                    {
                        case Button1:
                        {
                            mMouseButtonDown(this, e->x, e->y, MouseButton_Left);
                            break;
                        }
                        case Button2:
                        {
                            mMouseButtonDown(this, e->x, e->y, MouseButton_Middle);
                            break;
                        }
                        case Button3:
                        {
                            mMouseButtonDown(this, e->x, e->y, MouseButton_Right);
                            break;
                        }
                        case Button4:
                        {
                            mMouseWheel(this, e->x, e->y, 120);
                            break;
                        }
                        case Button5:
                        {
                            mMouseWheel(this, e->x, e->y, -120);
                            break;
                        }
                    }

                    break;
                }
                case ButtonRelease:
                {
                    XButtonEvent* e = (XButtonEvent*)&event;

                    switch (e->button)
                    {
                        case Button1:
                        {
                            mMouseButtonUp(this, e->x, e->y, MouseButton_Left);
                            break;
                        }
                        case Button2:
                        {
                            mMouseButtonUp(this, e->x, e->y, MouseButton_Middle);
                            break;
                        }
                        case Button3:
                        {
                            mMouseButtonUp(this, e->x, e->y, MouseButton_Right);
                            break;
                        }
                    }

                    break;
                }
                case KeyPress:
                {
                    XKeyEvent* e = (XKeyEvent*)&event;
                    KeySym ksym = XLookupKeysym(e, 0);
                    Key k = NoesisKey(ksym);
                    if (k != Key_None)
                    {
                        NS_LOG_TRACE("KEYDOWN: %s", XKeysymToString(ksym));
                        mKeyDown(this, k);
                    }

                    if (_xic != 0)
                    {
                        Status status;
                        char text[256];
                        int size = Xutf8LookupString(_xic, e, text, sizeof(text - 1), 0, &status);
                        if (size > 0 && (status == XLookupChars || status == XLookupBoth))
                        {
                            text[size] = 0;
                            NS_LOG_TRACE("TEXT: %s", text);

                            const char* text_ = text;
                            uint32_t c;
                            while ((c = UTF8::Next(text_)) != 0)
                            {
                                mChar(this, c);
                            }
                        }
                    }

                    break;
                }
                case KeyRelease:
                {
                    XKeyEvent* e = (XKeyEvent*)&event;
                    KeySym ksym = XLookupKeysym(e, 0);
                    Key k = NoesisKey(ksym);
                    if (k != Key_None)
                    {
                        NS_LOG_TRACE("KEYUP: %s", XKeysymToString(ksym));
                        mKeyUp(this, k);
                    }

                    break;
                }
            }
        }

        mRender(this);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void XDisplay::Close()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void* XDisplay::GetNativeHandle() const
{
    return (void*)&_x;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t XDisplay::GetClientWidth() const
{
    XWindowAttributes attrs;
    XGetWindowAttributes(_x.display, _x.window, &attrs);
    return attrs.width;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t XDisplay::GetClientHeight() const
{
    XWindowAttributes attrs;
    XGetWindowAttributes(_x.display, _x.window, &attrs);
    return attrs.height;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_IMPLEMENT_REFLECTION(XDisplay)
{
    NsMeta<TypeId>("XDisplay");
    NsMeta<Category>("Display");
}
