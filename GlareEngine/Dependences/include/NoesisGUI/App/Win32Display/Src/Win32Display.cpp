////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include "Win32Display.h"
#include "TSF.h"

#include <NsGui/IntegrationAPI.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/Log.h>
#include <NsCore/TypeId.h>
#include <NsCore/Category.h>
#include <NsCore/UTF8.h>
#include <NsCore/File.h>
#include <NsCore/FixedString.h>
#include <NsCore/HighResTimer.h>

#include <shellapi.h>
#include <ShellScalingApi.h>

#ifdef NS_PLATFORM_WINDOWS_DESKTOP
    #include <XInput.h>
    typedef DWORD (WINAPI *PFN_XINPUTGETSTATE)(DWORD, XINPUT_STATE*);
    static PFN_XINPUTGETSTATE XInputGetState_;
    typedef DWORD (WINAPI *PFN_XINPUTGETCAPABILITIES)(DWORD, DWORD, XINPUT_CAPABILITIES*);
    static PFN_XINPUTGETCAPABILITIES XInputGetCapabilities_;
    #define BUTTON_REPEAT_DELAY 0.250f
    #define BUTTON_REPEAT_RATE 1.0f / 31.0f
#endif

#define CLASS_NAME L"NoesisWindow"

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#define GET_POINTERID_WPARAM(wParam) (LOWORD(wParam))

#define WM_POINTERDOWN 0x0246
#define WM_POINTERUPDATE 0x0245
#define WM_POINTERUP 0x0247


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
Win32Display::Win32Display(): mWindowHandle(0), mStartupLocation(WindowStartupLocation_Manual),
    mCursor(Cursor_Arrow), mTrackingMouse(false)
{
#ifdef NS_PLATFORM_WINDOWS_DESKTOP
    HMODULE shCore = LoadLibraryA("Shcore.dll");
    if (shCore != 0)
    {
        typedef HRESULT (WINAPI* PFN_SETPROCESSDPIAWARENESS)(_In_ PROCESS_DPI_AWARENESS value);
        PFN_SETPROCESSDPIAWARENESS SetProcessDpiAwareness_ = (PFN_SETPROCESSDPIAWARENESS)
            GetProcAddress(shCore, "SetProcessDpiAwareness");
        if (SetProcessDpiAwareness_ != 0)
        {
            SetProcessDpiAwareness_(PROCESS_PER_MONITOR_DPI_AWARE);
        }

        FreeLibrary(shCore);
    }

    HMODULE xInput = LoadLibraryA("Xinput1_4.dll");
    if (xInput != 0)
    {
        XInputGetState_ = (PFN_XINPUTGETSTATE)GetProcAddress(xInput, "XInputGetState");
        XInputGetCapabilities_ = (PFN_XINPUTGETCAPABILITIES)GetProcAddress(xInput, "XInputGetCapabilities");
    }
#endif

    HINSTANCE instance = GetModuleHandle(0);

    // Register window class
    WNDCLASSEXW windowClass;

    if (GetClassInfoExW(instance, CLASS_NAME, &windowClass) == 0)
    {
        windowClass = {};
        windowClass.cbSize = sizeof(WNDCLASSEX);
        windowClass.style = CS_DBLCLKS;
        windowClass.lpfnWndProc = Win32Display::WndProc;
        windowClass.cbClsExtra = 0;
        windowClass.cbWndExtra = 0;
        windowClass.hInstance = instance;
        windowClass.hIcon = LoadIcon(instance, MAKEINTRESOURCE(101));
        windowClass.hIconSm = LoadIcon(instance, MAKEINTRESOURCE(101));
        windowClass.hCursor = 0;
        windowClass.hbrBackground = 0;
        windowClass.lpszMenuName = 0;
        windowClass.lpszClassName = CLASS_NAME;

        ATOM ret = RegisterClassExW(&windowClass);
        NS_ASSERT(ret != 0);
    }

    // Default flags
    uint32_t flags, flagsEx;
    BuildWin32StyleFlags(WindowStyle_SingleBorderWindow, WindowState_Normal, 
        ResizeMode_CanResize, true, false, false, flags, flagsEx);

    // Create window
    HWND wnd = CreateWindowExW(flagsEx, CLASS_NAME, L"", flags, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, this);
    NS_ASSERT(wnd != 0);
    NS_ASSERT(wnd == mWindowHandle);

    FillKeyTable();
    LoadCursors();
    TSF::Init();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Win32Display::~Win32Display()
{
    TSF::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::SetTitle(const char* title)
{
    uint16_t titleU16[512];
    uint32_t numChars = UTF8::UTF8To16(title, titleU16, 512);
    NS_ASSERT(numChars <= 512);

    BOOL ret = SetWindowTextW(mWindowHandle, (LPCWSTR)titleU16);
    NS_ASSERT(ret != 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::SetLocation(int x, int y)
{
    unsigned int flags = SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE;
    BOOL ret = SetWindowPos(mWindowHandle, 0, x, y, 0, 0, flags);
    NS_ASSERT(ret != 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::SetSize(uint32_t width, uint32_t height)
{
    unsigned int flags = SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE;
    BOOL ret = SetWindowPos(mWindowHandle, 0, 0, 0, width, height, flags);
    NS_ASSERT(ret != 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::SetWindowStyle(WindowStyle windowStyle)
{
    uint32_t flags = GetWin32StyleFlags();
    uint32_t flagsEx = GetWin32StyleFlagsEx();

    BuildWin32StyleFlags(windowStyle, GetState(flags), GetResizeMode(flags),
        GetShowInTaskbar(flagsEx), GetTopmost(flagsEx), GetAllowFileDrop(flagsEx), flags, flagsEx);

    SetWin32StyleFlags(flags);
    SetWin32StyleFlagsEx(flagsEx);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::SetWindowState(WindowState windowState)
{
    uint32_t flags = GetWin32StyleFlags();
    uint32_t flagsEx = GetWin32StyleFlagsEx();

    BuildWin32StyleFlags(GetStyle(flags, flagsEx), windowState, GetResizeMode(flags),
        GetShowInTaskbar(flagsEx), GetTopmost(flagsEx), GetAllowFileDrop(flagsEx), flags, flagsEx);

    SetWin32StyleFlags(flags);
    SetWin32StyleFlagsEx(flagsEx);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::SetResizeMode(ResizeMode resizeMode)
{
    uint32_t flags = GetWin32StyleFlags();
    uint32_t flagsEx = GetWin32StyleFlagsEx();

    BuildWin32StyleFlags(GetStyle(flags, flagsEx), GetState(flags), resizeMode,
        GetShowInTaskbar(flagsEx), GetTopmost(flagsEx), GetAllowFileDrop(flagsEx), flags, flagsEx);

    SetWin32StyleFlags(flags);
    SetWin32StyleFlagsEx(flagsEx);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::SetShowInTaskbar(bool showInTaskbar)
{
    uint32_t flags = GetWin32StyleFlags();
    uint32_t flagsEx = GetWin32StyleFlagsEx();

    BuildWin32StyleFlags(GetStyle(flags, flagsEx), GetState(flags), GetResizeMode(flags),
        showInTaskbar, GetTopmost(flagsEx), GetAllowFileDrop(flagsEx), flags, flagsEx);

    SetWin32StyleFlags(flags);
    SetWin32StyleFlagsEx(flagsEx);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::SetTopmost(bool topmost)
{
    HWND hWndInsertAfter = topmost ? HWND_TOPMOST : HWND_NOTOPMOST;
    unsigned int flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
    BOOL ret = SetWindowPos(mWindowHandle, hWndInsertAfter, 0, 0, 0, 0, flags);
    NS_ASSERT(ret != 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::SetAllowFileDrop(bool allowFileDrop)
{
    uint32_t flags = GetWin32StyleFlags();
    uint32_t flagsEx = GetWin32StyleFlagsEx();

    BuildWin32StyleFlags(GetStyle(flags, flagsEx), GetState(flags), GetResizeMode(flags),
        GetShowInTaskbar(flagsEx), GetTopmost(flagsEx), allowFileDrop, flags, flagsEx);

    SetWin32StyleFlags(flags);
    SetWin32StyleFlagsEx(flagsEx);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::SetWindowStartupLocation(WindowStartupLocation location)
{
    mStartupLocation = location;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::Show()
{
    switch (mStartupLocation)
    {
        case WindowStartupLocation_Manual:
        {
            break;
        }
        case WindowStartupLocation_CenterScreen:
        case WindowStartupLocation_CenterOwner:
        {
            CenterWindow();
            break;
        }
        default:
        {
            NS_ASSERT_UNREACHABLE;
        }
    }

    WindowState state = GetState(GetWin32StyleFlags());

    switch (state)
    {
        case WindowState_Maximized:
        {
            ShowWindow(mWindowHandle, SW_SHOWMAXIMIZED);
            break;
        }
        case WindowState_Minimized:
        {
            ShowWindow(mWindowHandle, SW_SHOWMINIMIZED);
            break;
        }
        case WindowState_Normal:
        {
            ShowWindow(mWindowHandle, SW_SHOWNORMAL);
            break;
        }
        default:
        {
            NS_ASSERT_UNREACHABLE;
        }
    }
}

namespace
{

////////////////////////////////////////////////////////////////////////////////////////////////////
bool GetNextMessage(bool wait, MSG& msg)
{
    if (wait)
    {
        BOOL ret = GetMessage(&msg, NULL, 0, 0);
        NS_ASSERT(ret != -1);
        return true;
    }
    else
    {
        return PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0;
    }
}

}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::EnterMessageLoop(bool runInBackground)
{
#ifdef NS_PLATFORM_WINDOWS_DESKTOP
    uint32_t inputState = 0;
    uint64_t lastEnumTime = 0;
    int32_t controllerIndex = -1;

    // Fake XInput buttons
    #define XINPUT_GAMEPAD_LTRIGGER_ 0x10000
    #define XINPUT_GAMEPAD_RTRIGGER_ 0x20000

    struct Key
    {
        DWORD button;
        Noesis::Key key;
        float ms;
    };

    Key keys[] =
    {
        { XINPUT_GAMEPAD_DPAD_LEFT, Key_GamepadLeft },
        { XINPUT_GAMEPAD_DPAD_RIGHT, Key_GamepadRight },
        { XINPUT_GAMEPAD_DPAD_UP, Key_GamepadUp },
        { XINPUT_GAMEPAD_DPAD_DOWN, Key_GamepadDown },
        { XINPUT_GAMEPAD_A, Key_GamepadAccept },
        { XINPUT_GAMEPAD_B, Key_GamepadCancel },
        { XINPUT_GAMEPAD_X, Key_GamepadContext1 },
        { XINPUT_GAMEPAD_Y, Key_GamepadContext2 },
        { XINPUT_GAMEPAD_LEFT_THUMB, Key_GamepadContext3 },
        { XINPUT_GAMEPAD_RIGHT_THUMB, Key_GamepadContext4 },
        { XINPUT_GAMEPAD_LEFT_SHOULDER, Key_GamepadPageLeft },
        { XINPUT_GAMEPAD_RIGHT_SHOULDER, Key_GamepadPageRight },
        { XINPUT_GAMEPAD_BACK, Key_GamepadView },
        { XINPUT_GAMEPAD_START, Key_GamepadMenu },
        { XINPUT_GAMEPAD_LTRIGGER_, Key_GamepadPageUp },
        { XINPUT_GAMEPAD_RTRIGGER_, Key_GamepadPageDown }
    };
#endif

#ifdef NS_PLATFORM_WINDOWS_DESKTOP
    uint64_t lastTime = HighResTimer::Ticks();
#endif

    for (;;)
    {
        MSG msg;
        bool sleep = !runInBackground && mWindowHandle != GetForegroundWindow();

        while (GetNextMessage(sleep, msg))
        {
            if (msg.message == WM_QUIT)
            {
                return;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
            sleep = false;
        }

#ifdef NS_PLATFORM_WINDOWS_DESKTOP
        if (controllerIndex == -1 && XInputGetCapabilities_)
        {
            uint64_t currentTime = ::GetTickCount64();
            if (currentTime - lastEnumTime > 2000)
            {
                lastEnumTime = currentTime;
                XINPUT_CAPABILITIES caps;

                for (uint32_t i = 0; i < 4; i++)
                {
                    uint32_t capsResult = XInputGetCapabilities_(i, XINPUT_FLAG_GAMEPAD, &caps);
                    if (capsResult == ERROR_SUCCESS)
                    {
                        NS_LOG_DEBUG("XInput controller #%d connected", i);
                        controllerIndex = i;
                        break;
                    }
                }
            }
        }

        uint64_t time = HighResTimer::Ticks();
        float dt = float(HighResTimer::Seconds(time - lastTime));
        lastTime = time;

        if (controllerIndex != -1)
        {
            XINPUT_STATE state_;
            if (XInputGetState_ && XInputGetState_(controllerIndex, &state_) == ERROR_SUCCESS)
            {
                int state = state_.Gamepad.wButtons;
                if (state_.Gamepad.sThumbLY > 26000) state |= XINPUT_GAMEPAD_DPAD_UP;
                if (state_.Gamepad.sThumbLY < -26000) state |= XINPUT_GAMEPAD_DPAD_DOWN;
                if (state_.Gamepad.sThumbLX > 26000) state |= XINPUT_GAMEPAD_DPAD_RIGHT;
                if (state_.Gamepad.sThumbLX < -26000) state |= XINPUT_GAMEPAD_DPAD_LEFT;
                if (state_.Gamepad.bLeftTrigger > 200) state |= XINPUT_GAMEPAD_LTRIGGER_;
                if (state_.Gamepad.bRightTrigger > 200) state |= XINPUT_GAMEPAD_RTRIGGER_;

                if (abs(state_.Gamepad.sThumbRY) > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
                {
                    mScroll(this, (float)state_.Gamepad.sThumbRY / 32767);
                }

                if (abs(state_.Gamepad.sThumbRX) > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
                {
                    mHScroll(this, (float)state_.Gamepad.sThumbRX / 32767);
                }

                uint32_t delta = inputState ^ state;
                if (delta != 0 || state != 0)
                {
                    for (size_t i = 0; i < NS_COUNTOF(keys); i++)
                    {
                        if (delta & keys[i].button)
                        {
                            if (state & keys[i].button)
                            {
                                mKeyDown(this, keys[i].key);
                                keys[i].ms = BUTTON_REPEAT_DELAY;
                            }
                            else
                            {
                                mKeyUp(this, keys[i].key);
                            }
                        }
                        else if (state & keys[i].button)
                        {
                            keys[i].ms -= dt;
                            if (keys[i].ms < 0.0f)
                            {
                                mKeyDown(this, keys[i].key);
                                keys[i].ms = BUTTON_REPEAT_RATE;
                            }
                        }
                    }
                }


                inputState = state;
            }
            else
            {
                NS_LOG_DEBUG("XInput controller #%d disconnected", controllerIndex);
                controllerIndex = -1;
            }
        }
#endif

        mRender(this);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::Close()
{
    SendMessage(mWindowHandle, WM_CLOSE, 0, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::OpenSoftwareKeyboard(Noesis::UIElement* focused)
{
    TSF::ShowKeyboard(focused);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::CloseSoftwareKeyboard()
{
    TSF::HideKeyboard();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::SetCursor(Noesis::Cursor cursor)
{
    mCursor = cursor;

    // As explained in MSDN documentation WM_SETCURSOR message is not sent if mouse is captured
    // by a window, so we have to manually send the message here otherwise drag & drop cursor
    // changes won't be visible
    if (GetCapture() != NULL)
    {
        SendMessage(mWindowHandle, WM_SETCURSOR, (WPARAM)0, (LPARAM)HTCLIENT);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::OpenUrl(const char* url)
{
    if ((int)(uintptr_t)ShellExecuteA(0, 0, url, 0, 0, SW_SHOW) <= 32)
    {
        // If the link does not open it is probably a local URL. Try using the exe path
        WCHAR exe16[MAX_PATH];
        GetModuleFileNameW(nullptr, exe16, MAX_PATH);

        char exe[MAX_PATH];
        UTF8::UTF16To8((uint16_t*)exe16, exe, MAX_PATH);

        FixedPath path = File::DirName(exe);
        path += "/";
        path += url;

        ShellExecuteA(0, 0, path.c_str(), 0, 0, SW_SHOW);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void* Win32Display::GetNativeHandle() const
{
    return mWindowHandle;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t Win32Display::GetClientWidth() const
{
    RECT rect;
    BOOL ret = GetClientRect(mWindowHandle, &rect);
    NS_ASSERT(ret != 0);
    return rect.right;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t Win32Display::GetClientHeight() const
{
    RECT rect;
    BOOL ret = GetClientRect(mWindowHandle, &rect);
    NS_ASSERT(ret != 0);
    return rect.bottom;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::FillKeyTable()
{
    memset(mKeyTable, 0, sizeof(mKeyTable));

    mKeyTable[VK_BACK] = Key_Back;
    mKeyTable[VK_TAB] = Key_Tab;
    mKeyTable[VK_CLEAR] = Key_Clear;
    mKeyTable[VK_RETURN] = Key_Return;
    mKeyTable[VK_PAUSE] = Key_Pause;

    mKeyTable[VK_SHIFT] = Key_LeftShift;
    mKeyTable[VK_LSHIFT] = Key_LeftShift;
    mKeyTable[VK_RSHIFT] = Key_RightShift;
    mKeyTable[VK_CONTROL] = Key_LeftCtrl;
    mKeyTable[VK_LCONTROL] = Key_LeftCtrl;
    mKeyTable[VK_RCONTROL] = Key_RightCtrl;
    mKeyTable[VK_MENU] = Key_LeftAlt;
    mKeyTable[VK_LMENU] = Key_LeftAlt;
    mKeyTable[VK_RMENU] = Key_RightAlt;
    mKeyTable[VK_LWIN] = Key_LWin;
    mKeyTable[VK_RWIN] = Key_RWin;
    mKeyTable[VK_ESCAPE] = Key_Escape;

    mKeyTable[VK_SPACE] = Key_Space;
    mKeyTable[VK_PRIOR] = Key_Prior;
    mKeyTable[VK_NEXT] = Key_Next;
    mKeyTable[VK_END] = Key_End;
    mKeyTable[VK_HOME] = Key_Home;
    mKeyTable[VK_LEFT] = Key_Left;
    mKeyTable[VK_UP] = Key_Up;
    mKeyTable[VK_RIGHT] = Key_Right;
    mKeyTable[VK_DOWN] = Key_Down;
    mKeyTable[VK_SELECT] = Key_Select;
    mKeyTable[VK_PRINT] = Key_Print;
    mKeyTable[VK_EXECUTE] = Key_Execute;
    mKeyTable[VK_SNAPSHOT] = Key_Snapshot;
    mKeyTable[VK_INSERT] = Key_Insert;
    mKeyTable[VK_DELETE] = Key_Delete;
    mKeyTable[VK_HELP] = Key_Help;

    mKeyTable['0'] = Key_D0;
    mKeyTable['1'] = Key_D1;
    mKeyTable['2'] = Key_D2;
    mKeyTable['3'] = Key_D3;
    mKeyTable['4'] = Key_D4;
    mKeyTable['5'] = Key_D5;
    mKeyTable['6'] = Key_D6;
    mKeyTable['7'] = Key_D7;
    mKeyTable['8'] = Key_D8;
    mKeyTable['9'] = Key_D9;

    mKeyTable[VK_NUMPAD0] = Key_NumPad0;
    mKeyTable[VK_NUMPAD1] = Key_NumPad1;
    mKeyTable[VK_NUMPAD2] = Key_NumPad2;
    mKeyTable[VK_NUMPAD3] = Key_NumPad3;
    mKeyTable[VK_NUMPAD4] = Key_NumPad4;
    mKeyTable[VK_NUMPAD5] = Key_NumPad5;
    mKeyTable[VK_NUMPAD6] = Key_NumPad6;
    mKeyTable[VK_NUMPAD7] = Key_NumPad7;
    mKeyTable[VK_NUMPAD8] = Key_NumPad8;
    mKeyTable[VK_NUMPAD9] = Key_NumPad9;

    mKeyTable[VK_MULTIPLY] = Key_Multiply;
    mKeyTable[VK_ADD] = Key_Add;
    mKeyTable[VK_SEPARATOR] = Key_Separator;
    mKeyTable[VK_SUBTRACT] = Key_Subtract;
    mKeyTable[VK_DECIMAL] = Key_Decimal;
    mKeyTable[VK_DIVIDE] = Key_Divide;

    mKeyTable['A'] = Key_A;
    mKeyTable['B'] = Key_B;
    mKeyTable['C'] = Key_C;
    mKeyTable['D'] = Key_D;
    mKeyTable['E'] = Key_E;
    mKeyTable['F'] = Key_F;
    mKeyTable['G'] = Key_G;
    mKeyTable['H'] = Key_H;
    mKeyTable['I'] = Key_I;
    mKeyTable['J'] = Key_J;
    mKeyTable['K'] = Key_K;
    mKeyTable['L'] = Key_L;
    mKeyTable['M'] = Key_M;
    mKeyTable['N'] = Key_N;
    mKeyTable['O'] = Key_O;
    mKeyTable['P'] = Key_P;
    mKeyTable['Q'] = Key_Q;
    mKeyTable['R'] = Key_R;
    mKeyTable['S'] = Key_S;
    mKeyTable['T'] = Key_T;
    mKeyTable['U'] = Key_U;
    mKeyTable['V'] = Key_V;
    mKeyTable['W'] = Key_W;
    mKeyTable['X'] = Key_X;
    mKeyTable['Y'] = Key_Y;
    mKeyTable['Z'] = Key_Z;

    mKeyTable[VK_F1] = Key_F1;
    mKeyTable[VK_F2] = Key_F2;
    mKeyTable[VK_F3] = Key_F3;
    mKeyTable[VK_F4] = Key_F4;
    mKeyTable[VK_F5] = Key_F5;
    mKeyTable[VK_F6] = Key_F6;
    mKeyTable[VK_F7] = Key_F7;
    mKeyTable[VK_F8] = Key_F8;
    mKeyTable[VK_F9] = Key_F9;
    mKeyTable[VK_F10] = Key_F10;
    mKeyTable[VK_F11] = Key_F11;
    mKeyTable[VK_F12] = Key_F12;
    mKeyTable[VK_F13] = Key_F13;
    mKeyTable[VK_F14] = Key_F14;
    mKeyTable[VK_F15] = Key_F15;
    mKeyTable[VK_F16] = Key_F16;
    mKeyTable[VK_F17] = Key_F17;
    mKeyTable[VK_F18] = Key_F18;
    mKeyTable[VK_F19] = Key_F19;
    mKeyTable[VK_F20] = Key_F20;
    mKeyTable[VK_F21] = Key_F21;
    mKeyTable[VK_F22] = Key_F22;
    mKeyTable[VK_F23] = Key_F23;
    mKeyTable[VK_F24] = Key_F24;

    mKeyTable[VK_NUMLOCK] = Key_NumLock;
    mKeyTable[VK_SCROLL] = Key_Scroll;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::LoadCursors()
{
    memset(mCursors, 0, sizeof(mCursors));

    mCursors[Cursor_No] = LoadCursor(0, IDC_NO);
    mCursors[Cursor_Arrow] = LoadCursor(0, IDC_ARROW);
    mCursors[Cursor_AppStarting] = LoadCursor(0, IDC_APPSTARTING);
    mCursors[Cursor_Cross] = LoadCursor(0, IDC_CROSS);
    mCursors[Cursor_Help] = LoadCursor(0, IDC_HELP);
    mCursors[Cursor_IBeam] = LoadCursor(0, IDC_IBEAM);
    mCursors[Cursor_SizeAll] = LoadCursor(0, IDC_SIZEALL);
    mCursors[Cursor_SizeNESW] = LoadCursor(0, IDC_SIZENESW);
    mCursors[Cursor_SizeNS] = LoadCursor(0, IDC_SIZENS);
    mCursors[Cursor_SizeNWSE] = LoadCursor(0, IDC_SIZENWSE);
    mCursors[Cursor_SizeWE] = LoadCursor(0, IDC_SIZEWE);
    mCursors[Cursor_Wait] = LoadCursor(0, IDC_WAIT);
    mCursors[Cursor_Hand] = LoadCursor(0, IDC_HAND);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::BuildWin32StyleFlags(WindowStyle style, WindowState state, ResizeMode resize,
    bool showInTaskbar, bool topmost, bool allowDragFiles, uint32_t& flags,
    uint32_t& flagsEx) const
{
    flags = 0;
    flagsEx = allowDragFiles ? WS_EX_ACCEPTFILES : 0;

    switch (style)
    {
        case WindowStyle_None:
        {
            flags |= WS_POPUP;
            break;
        }
        case WindowStyle_SingleBorderWindow:
        {
            flags |= WS_CAPTION | WS_SYSMENU;
            flagsEx |= WS_EX_WINDOWEDGE;
            break;
        }
        case WindowStyle_ThreeDBorderWindow:
        {
            flags |= WS_CAPTION | WS_SYSMENU;
            flagsEx |= WS_EX_CLIENTEDGE;
            break;
        }
        case WindowStyle_ToolWindow:
        {
            flags |= WS_CAPTION;
            flagsEx |= WS_EX_TOOLWINDOW;
            break;
        }
        default:
        {
            NS_ASSERT_UNREACHABLE;
        }
    }

    switch (state)
    {
        case WindowState_Maximized:
        {
            flags |= WS_MAXIMIZE;
            break;
        }
        case WindowState_Minimized:
        {
            flags |= WS_MINIMIZE;
            break;
        }
        case WindowState_Normal:
        {
            break;
        }
        default:
        {
            NS_ASSERT_UNREACHABLE;
        }
    }

    switch (resize)
    {
        case ResizeMode_CanMinimize:
        {
            flags |= WS_MINIMIZEBOX;
            break;
        }
        case ResizeMode_CanResize:
        case ResizeMode_CanResizeWithGrip:
        {
            flags |= WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
            break;
        }
        case ResizeMode_NoResize:
        {
            break;
        }
        default:
        {
            NS_ASSERT_UNREACHABLE;
        }
    }

    if (showInTaskbar)
    {
        flagsEx |= WS_EX_APPWINDOW;
    }
    else
    {
        flagsEx |= WS_EX_NOACTIVATE;
    }

    if (topmost)
    {
        flagsEx |= WS_EX_TOPMOST;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t Win32Display::GetWin32StyleFlags() const
{
    SetLastError(0);
    LONG flags = GetWindowLong(mWindowHandle, GWL_STYLE);
    NS_ASSERT(flags != 0 || GetLastError() == 0);

    return flags;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t Win32Display::GetWin32StyleFlagsEx() const
{
    SetLastError(0);
    LONG flags = GetWindowLong(mWindowHandle, GWL_EXSTYLE);
    NS_ASSERT(flags != 0 || GetLastError() == 0);

    return flags;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::SetWin32StyleFlags(uint32_t flags) const
{
    SetLastError(0);
    LONG_PTR prev = SetWindowLongPtr(mWindowHandle, GWL_STYLE, (__int3264)(LONG_PTR)(flags));
    NS_ASSERT(prev != 0 || GetLastError() == 0);

    unsigned int uFlags = SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED;
    BOOL ret = SetWindowPos(mWindowHandle, 0, 0, 0, 0, 0, uFlags);
    NS_ASSERT(ret != 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::SetWin32StyleFlagsEx(uint32_t flags) const
{
    SetLastError(0);
    LONG_PTR prev = SetWindowLongPtr(mWindowHandle, GWL_EXSTYLE, (__int3264)(LONG_PTR)(flags));
    NS_ASSERT(prev != 0 || GetLastError() == 0);

    unsigned int uFlags = SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED;
    BOOL ret = SetWindowPos(mWindowHandle, 0, 0, 0, 0, 0, uFlags);
    NS_ASSERT(ret != 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
WindowStyle Win32Display::GetStyle(uint32_t flags, uint32_t flagsEx) const
{
    if ((flags & WS_CAPTION) != 0)
    {
        if ((flagsEx & WS_EX_TOOLWINDOW) != 0)
        {
            return WindowStyle_ToolWindow;
        }
        else if ((flags & WS_SYSMENU) != 0 && (flagsEx & WS_EX_CLIENTEDGE) != 0)
        {
            return WindowStyle_ThreeDBorderWindow;
        }
    }
    else if ((flags & WS_POPUP) != 0)
    {
        return WindowStyle_None;
    }

    return WindowStyle_SingleBorderWindow;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
WindowState Win32Display::GetState(uint32_t flags) const
{
    if ((flags & WS_MAXIMIZE) != 0)
    {
        return WindowState_Maximized;
    }
    else if ((flags & WS_MINIMIZE) != 0)
    {
        return WindowState_Minimized;
    }

    return WindowState_Normal;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
ResizeMode Win32Display::GetResizeMode(uint32_t flags) const
{
    if ((flags & WS_MINIMIZEBOX) != 0)
    {
        if ((flags & WS_THICKFRAME) != 0 && (flags & WS_MAXIMIZEBOX) != 0)
        {
            return ResizeMode_CanResize;
        }
        else
        {
            return ResizeMode_CanMinimize;
        }
    }

    return ResizeMode_NoResize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Win32Display::GetShowInTaskbar(uint32_t flagsEx) const
{
    if ((flagsEx & WS_EX_APPWINDOW) != 0)
    {
        return true;
    }
    else if ((flagsEx & WS_EX_NOACTIVATE) != 0)
    {
        return false;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Win32Display::GetTopmost(uint32_t flagsEx) const
{
    return (flagsEx & WS_EX_TOPMOST) != 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Win32Display::GetAllowFileDrop(uint32_t flagsEx) const
{
    return (flagsEx & WS_EX_ACCEPTFILES) != 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Win32Display::CenterWindow()
{
    HWND desktop = GetDesktopWindow();
    HMONITOR monitor = MonitorFromWindow(desktop, MONITOR_DEFAULTTOPRIMARY);

    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    BOOL ret = GetMonitorInfo(monitor, &info);
    NS_ASSERT(ret != 0);

    LONG monitorWidth = info.rcMonitor.right - info.rcMonitor.left;
    LONG monitorHeight = info.rcMonitor.bottom - info.rcMonitor.top;

    RECT r;
    ret = GetWindowRect(mWindowHandle, &r);
    NS_ASSERT(ret != 0);

    LONG width = r.right - r.left;
    LONG height = r.bottom - r.top;

    int x = (int)(0.5f * (monitorWidth - width));
    int y = (int)(0.5f * (monitorHeight - height));
    SetLocation(x, y);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Win32Display::DispatchEvent(unsigned int msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_SETCURSOR:
        {
            if (LOWORD(lParam) == HTCLIENT)
            {
                if (mCursors[mCursor] != 0)
                {
                    ::SetCursor(mCursors[mCursor]);
                    return true;
                }
            }

            return false;
        }
        case WM_MOVE:
        {
            RECT r;
            BOOL ret = GetWindowRect(mWindowHandle, &r);
            NS_ASSERT(ret != 0);

            mLocationChanged(this, r.left, r.top);
            TSF::MoveWindow(mWindowHandle);
            return false;
        }
        case WM_EXITSIZEMOVE:
        {
            TSF::MoveWindow(mWindowHandle);
            return false;
        }
        case WM_SIZE:
        {
            RECT r;
            BOOL ret = GetWindowRect(mWindowHandle, &r);
            NS_ASSERT(ret != 0);

            uint32_t width = r.right - r.left;
            uint32_t height = r.bottom - r.top;

            switch (wParam)
            {
                case SIZE_RESTORED:
                {
                    mStateChanged(this, WindowState_Normal);
                    mSizeChanged(this, width, height);
                    break;
                }
                case SIZE_MAXIMIZED:
                {
                    mStateChanged(this, WindowState_Maximized);
                    mSizeChanged(this, width, height);
                    break;
                }
                case SIZE_MINIMIZED:
                {
                    mStateChanged(this, WindowState_Minimized);
                    break;
                }
            }

            return false;
        }
        case WM_ACTIVATE:
        {
            switch (LOWORD(wParam))
            {
                case WA_INACTIVE:
                {
                    mDeactivated(this);
                    TSF::DeactivateWindow(mWindowHandle);
                    break;
                }

                default:
                {
                    TSF::ActivateWindow(mWindowHandle);
                    mActivated(this);

                    // Sync key states (they are reset each time window is deactivated)
                    for (uint32_t i = 0; i < NS_COUNTOF(mKeyTable); i++)
                    {
                        if (mKeyTable[i] != 0 && GetAsyncKeyState(i))
                        {
                            mKeyDown(this, (Key)mKeyTable[i]);
                        }
                    }

                    break;
                }
            }

            return false;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return false;
        }
        case WM_DROPFILES:
        {
            HDROP hDrop = (HDROP)wParam;
            unsigned int numFiles = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);

            for (unsigned int i = 0; i < numFiles; ++i)
            {
                WCHAR filenameU32[MAX_PATH];
                if (DragQueryFileW(hDrop, i, filenameU32, MAX_PATH) != 0)
                {
                    char filename[MAX_PATH];
                    uint32_t numChars = UTF8::UTF16To8((uint16_t*)filenameU32, filename, MAX_PATH);
                    NS_ASSERT(numChars <= MAX_PATH);

                    mFileDropped(this, filename);
                }
            }

            DragFinish(hDrop);
            SetForegroundWindow(mWindowHandle);

            return false;
        }
        // BEGIN - Mouse Input Notifications
        case WM_MOUSELEAVE:
        {
            mTrackingMouse = false;
            POINT mousePos;
            GetCursorPos(&mousePos);
            ScreenToClient(mWindowHandle, &mousePos);

            // Make sure mouse position is outside client area
            if (mousePos.x > 0) mousePos.x += 1;
            if (mousePos.y > 0) mousePos.y += 1;

            mMouseMove(this, mousePos.x, mousePos.y);
            return true;
        }
        case WM_MOUSEMOVE:
        {
            if (!mTrackingMouse)
            {
                TRACKMOUSEEVENT trackMouseEvent =
                {
                    sizeof(TRACKMOUSEEVENT), TME_LEAVE, mWindowHandle, HOVER_DEFAULT
                };
                TrackMouseEvent(&trackMouseEvent);
                mTrackingMouse = true;
            }

            mMouseMove(this, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return false;
        }
        case WM_LBUTTONDOWN:
        {
            SetCapture(mWindowHandle);
            SetFocus(mWindowHandle);

            mMouseButtonDown(this, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), MouseButton_Left);
            return false;
        }
        case WM_MBUTTONDOWN:
        {
            SetCapture(mWindowHandle);
            SetFocus(mWindowHandle);
            mMouseButtonDown(this, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), MouseButton_Middle);
            return false;
        }
        case WM_RBUTTONDOWN:
        {
            SetCapture(mWindowHandle);
            SetFocus(mWindowHandle);
            mMouseButtonDown(this, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), MouseButton_Right);
            return false;
        }
        case WM_LBUTTONUP:
        {
            mMouseButtonUp(this, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), MouseButton_Left);
            ReleaseCapture();
            return false;
        }
        case WM_MBUTTONUP:
        {
            mMouseButtonUp(this, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), MouseButton_Middle);
            ReleaseCapture();
            return false;
        }
        case WM_RBUTTONUP:
        {
            mMouseButtonUp(this, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), MouseButton_Right);
            ReleaseCapture();
            return false;
        }
        case WM_LBUTTONDBLCLK:
        {
            mMouseDoubleClick(this, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), MouseButton_Left);
            return false;
        }
        case WM_RBUTTONDBLCLK:
        {
            mMouseDoubleClick(this, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), MouseButton_Right);
            return false;
        }
        case WM_MBUTTONDBLCLK:
        {
            mMouseDoubleClick(this, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), MouseButton_Middle);
            return false;
        }
        case WM_MOUSEWHEEL:
        {
            // Mouse wheel event receives mouse position in screen coordinates
            POINT point;
            point.x = GET_X_LPARAM(lParam);
            point.y = GET_Y_LPARAM(lParam);
            ScreenToClient(mWindowHandle, &point);

            mMouseWheel(this, point.x, point.y, GET_WHEEL_DELTA_WPARAM(wParam));
            return false;
        }
        case WM_MOUSEHWHEEL:
        {
            // Mouse wheel event receives mouse position in screen coordinates
            POINT point;
            point.x = GET_X_LPARAM(lParam);
            point.y = GET_Y_LPARAM(lParam);
            ScreenToClient(mWindowHandle, &point);

            mMouseHWheel(this, point.x, point.y, GET_WHEEL_DELTA_WPARAM(wParam));
            return false;
        }
        // END - Mouse Input Notifications
        // BEGIN -- Keyboard Input Notifications
        case WM_KEYDOWN:
        {
            if (wParam <= 0xff && mKeyTable[wParam] != 0)
            {
                mKeyDown(this, (Key)mKeyTable[wParam]);
            }

            return false;
        }
        case WM_KEYUP:
        {
            if (wParam <= 0xff && mKeyTable[wParam] != 0)
            {
                mKeyUp(this, (Key)mKeyTable[wParam]);
            }

            return false;
        }
        case WM_SYSKEYDOWN:
        {
            if (wParam <= 0xff && mKeyTable[wParam] != 0)
            {
                mKeyDown(this, (Key)mKeyTable[wParam]);
            }

            return false;
        }
        case WM_SYSKEYUP:
        {
            if (wParam <= 0xff && mKeyTable[wParam] != 0)
            {
                mKeyUp(this, (Key)mKeyTable[wParam]);
            }

            return false;
        }
        case WM_CHAR:
        {
            mChar(this, (uint32_t)wParam);
            return false;
        }
        // END --Keyboard Input Notifications
        // BEGIN -- Touch Input Notifications
        case WM_POINTERDOWN:
        {
            uint32_t id = GET_POINTERID_WPARAM(wParam);
            POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(mWindowHandle, &point);
            mTouchDown(this, point.x, point.y, id);
            return true;
        }
        case WM_POINTERUPDATE:
        {
            uint32_t id = GET_POINTERID_WPARAM(wParam);
            POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(mWindowHandle, &point);
            mTouchMove(this, point.x, point.y, id);
            return true;
        }
        case WM_POINTERUP:
        {
            uint32_t id = GET_POINTERID_WPARAM(wParam);
            POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(mWindowHandle, &point);
            mTouchUp(this, point.x, point.y, id);
            return true;
        }
        // END -- Touch Input Notifications
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK Win32Display::WndProc(HWND hWnd, unsigned int msg, WPARAM wParam, LPARAM lParam)
{
    Win32Display* display = 0;

    if (msg == WM_NCCREATE)
    {
        display = (Win32Display*)(((LPCREATESTRUCT)(lParam))->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (__int3264)(LONG_PTR)(display));
        display->mWindowHandle = hWnd;
    }
    else
    {
        display = (Win32Display*)((LONG_PTR)GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (display && display->DispatchEvent(msg, wParam, lParam))
    {
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(Win32Display)
{
    NsMeta<TypeId>("Win32Display");
    NsMeta<Category>("Display");
}
