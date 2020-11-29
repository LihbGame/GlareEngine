////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include "EmscriptenDisplay.h"

#include <NsCore/ReflectionImplement.h>
#include <NsCore/Category.h>

#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/key_codes.h>


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
EmscriptenDisplay::EmscriptenDisplay(): mWidth(0), mHeight(0)
{
    FillKeyTable();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EmscriptenDisplay::FillKeyTable()
{
    memset(mKeyTable, 0, sizeof(mKeyTable));

    mKeyTable[DOM_VK_BACK_SPACE] = Key_Back;
    mKeyTable[DOM_VK_TAB] = Key_Tab;
    mKeyTable[DOM_VK_CLEAR] = Key_Clear;
    mKeyTable[DOM_VK_RETURN] = Key_Return;
    mKeyTable[DOM_VK_PAUSE] = Key_Pause;

    mKeyTable[DOM_VK_SHIFT] = Key_LeftShift;
    mKeyTable[DOM_VK_CONTROL] = Key_LeftCtrl;
    mKeyTable[DOM_VK_CONTEXT_MENU] = Key_LeftAlt;
    mKeyTable[DOM_VK_ESCAPE] = Key_Escape;

    mKeyTable[DOM_VK_SPACE] = Key_Space;
    mKeyTable[DOM_VK_PAGE_UP] = Key_Prior;
    mKeyTable[DOM_VK_PAGE_DOWN] = Key_Next;
    mKeyTable[DOM_VK_END] = Key_End;
    mKeyTable[DOM_VK_HOME] = Key_Home;
    mKeyTable[DOM_VK_LEFT] = Key_Left;
    mKeyTable[DOM_VK_UP] = Key_Up;
    mKeyTable[DOM_VK_RIGHT] = Key_Right;
    mKeyTable[DOM_VK_DOWN] = Key_Down;
    mKeyTable[DOM_VK_SELECT] = Key_Select;
    mKeyTable[DOM_VK_PRINT] = Key_Print;
    mKeyTable[DOM_VK_EXECUTE] = Key_Execute;
    mKeyTable[DOM_VK_PRINTSCREEN] = Key_Snapshot;
    mKeyTable[DOM_VK_INSERT] = Key_Insert;
    mKeyTable[DOM_VK_DELETE] = Key_Delete;
    mKeyTable[DOM_VK_HELP] = Key_Help;

    mKeyTable[DOM_VK_0] = Key_D0;
    mKeyTable[DOM_VK_1] = Key_D1;
    mKeyTable[DOM_VK_2] = Key_D2;
    mKeyTable[DOM_VK_3] = Key_D3;
    mKeyTable[DOM_VK_4] = Key_D4;
    mKeyTable[DOM_VK_5] = Key_D5;
    mKeyTable[DOM_VK_6] = Key_D6;
    mKeyTable[DOM_VK_7] = Key_D7;
    mKeyTable[DOM_VK_8] = Key_D8;
    mKeyTable[DOM_VK_9] = Key_D9;

    mKeyTable[DOM_VK_NUMPAD0] = Key_NumPad0;
    mKeyTable[DOM_VK_NUMPAD1] = Key_NumPad1;
    mKeyTable[DOM_VK_NUMPAD2] = Key_NumPad2;
    mKeyTable[DOM_VK_NUMPAD3] = Key_NumPad3;
    mKeyTable[DOM_VK_NUMPAD4] = Key_NumPad4;
    mKeyTable[DOM_VK_NUMPAD5] = Key_NumPad5;
    mKeyTable[DOM_VK_NUMPAD6] = Key_NumPad6;
    mKeyTable[DOM_VK_NUMPAD7] = Key_NumPad7;
    mKeyTable[DOM_VK_NUMPAD8] = Key_NumPad8;
    mKeyTable[DOM_VK_NUMPAD9] = Key_NumPad9;

    mKeyTable[DOM_VK_MULTIPLY] = Key_Multiply;
    mKeyTable[DOM_VK_ADD] = Key_Add;
    mKeyTable[DOM_VK_SEPARATOR] = Key_Separator;
    mKeyTable[DOM_VK_SUBTRACT] = Key_Subtract;
    mKeyTable[DOM_VK_DECIMAL] = Key_Decimal;
    mKeyTable[DOM_VK_DIVIDE] = Key_Divide;

    mKeyTable[DOM_VK_A] = Key_A;
    mKeyTable[DOM_VK_B] = Key_B;
    mKeyTable[DOM_VK_C] = Key_C;
    mKeyTable[DOM_VK_D] = Key_D;
    mKeyTable[DOM_VK_E] = Key_E;
    mKeyTable[DOM_VK_F] = Key_F;
    mKeyTable[DOM_VK_G] = Key_G;
    mKeyTable[DOM_VK_H] = Key_H;
    mKeyTable[DOM_VK_I] = Key_I;
    mKeyTable[DOM_VK_J] = Key_J;
    mKeyTable[DOM_VK_K] = Key_K;
    mKeyTable[DOM_VK_L] = Key_L;
    mKeyTable[DOM_VK_M] = Key_M;
    mKeyTable[DOM_VK_N] = Key_N;
    mKeyTable[DOM_VK_O] = Key_O;
    mKeyTable[DOM_VK_P] = Key_P;
    mKeyTable[DOM_VK_Q] = Key_Q;
    mKeyTable[DOM_VK_R] = Key_R;
    mKeyTable[DOM_VK_S] = Key_S;
    mKeyTable[DOM_VK_T] = Key_T;
    mKeyTable[DOM_VK_U] = Key_U;
    mKeyTable[DOM_VK_V] = Key_V;
    mKeyTable[DOM_VK_W] = Key_W;
    mKeyTable[DOM_VK_X] = Key_X;
    mKeyTable[DOM_VK_Y] = Key_Y;
    mKeyTable[DOM_VK_Z] = Key_Z;

    mKeyTable[DOM_VK_F1] = Key_F1;
    mKeyTable[DOM_VK_F2] = Key_F2;
    mKeyTable[DOM_VK_F3] = Key_F3;
    mKeyTable[DOM_VK_F4] = Key_F4;
    mKeyTable[DOM_VK_F5] = Key_F5;
    mKeyTable[DOM_VK_F6] = Key_F6;
    mKeyTable[DOM_VK_F7] = Key_F7;
    mKeyTable[DOM_VK_F8] = Key_F8;
    mKeyTable[DOM_VK_F9] = Key_F9;
    mKeyTable[DOM_VK_F10] = Key_F10;
    mKeyTable[DOM_VK_F11] = Key_F11;
    mKeyTable[DOM_VK_F12] = Key_F12;
    mKeyTable[DOM_VK_F13] = Key_F13;
    mKeyTable[DOM_VK_F14] = Key_F14;
    mKeyTable[DOM_VK_F15] = Key_F15;
    mKeyTable[DOM_VK_F16] = Key_F16;
    mKeyTable[DOM_VK_F17] = Key_F17;
    mKeyTable[DOM_VK_F18] = Key_F18;
    mKeyTable[DOM_VK_F19] = Key_F19;
    mKeyTable[DOM_VK_F20] = Key_F20;
    mKeyTable[DOM_VK_F21] = Key_F21;
    mKeyTable[DOM_VK_F22] = Key_F22;
    mKeyTable[DOM_VK_F23] = Key_F23;
    mKeyTable[DOM_VK_F24] = Key_F24;

    mKeyTable[DOM_VK_NUM_LOCK] = Key_NumLock;
    mKeyTable[DOM_VK_SCROLL_LOCK] = Key_Scroll;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EmscriptenDisplay::UpdateSize()
{
    double width, height;
    EMSCRIPTEN_RESULT r = emscripten_get_element_css_size("#canvas", &width, &height);
    NS_ASSERT(r == EMSCRIPTEN_RESULT_SUCCESS);

    mWidth = (uint32_t)width;
    mHeight = (uint32_t)height;
    r = emscripten_set_canvas_element_size("#canvas", mWidth, mHeight);
    NS_ASSERT(r == EMSCRIPTEN_RESULT_SUCCESS);

    mLocationChanged(this, 0, 0);
    mSizeChanged(this, mWidth, mHeight);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static MouseButton NoesisButton(unsigned short button)
{
    switch (button)
    {
        case 0: return MouseButton_Left;
        case 1: return MouseButton_Middle;
        case 2: return MouseButton_Right;
        default: return MouseButton_XButton1;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EmscriptenDisplay::Show()
{
    UpdateSize();

    emscripten_set_resize_callback(nullptr, this, false,
        [](int, const EmscriptenUiEvent*, void* user) -> EMSCRIPTEN_RESULT
        {
            ((EmscriptenDisplay*)user)->UpdateSize();
            return EMSCRIPTEN_RESULT_SUCCESS;
        });

    emscripten_set_mousedown_callback("#canvas", this, true, 
        [](int, const EmscriptenMouseEvent* e, void* user) -> EMSCRIPTEN_RESULT
        {
            EmscriptenDisplay* display = (EmscriptenDisplay*)user;
            int x = e->canvasX;
            int y = e->canvasY;
            MouseButton button = NoesisButton(e->button);
            display->mMouseButtonDown(display, x, y, button);
            return true;
        });

    emscripten_set_mouseup_callback("#canvas", this, true, 
        [](int, const EmscriptenMouseEvent* e, void* user) -> EMSCRIPTEN_RESULT
        {
            EmscriptenDisplay* display = (EmscriptenDisplay*)user;
            int x = e->canvasX;
            int y = e->canvasY;
            MouseButton button = NoesisButton(e->button);
            display->mMouseButtonUp(display, x, y, button);
            return true;
        });

    emscripten_set_mousemove_callback("#canvas", this, true, 
        [](int, const EmscriptenMouseEvent* e, void* user) -> EMSCRIPTEN_RESULT
        {
            EmscriptenDisplay* display = (EmscriptenDisplay*)user;
            int x = e->canvasX;
            int y = e->canvasY;
            display->mMouseMove(display, x, y);
            return true;
        });

    emscripten_set_wheel_callback("#canvas", this, true, 
        [](int, const EmscriptenWheelEvent* e, void* user) -> EMSCRIPTEN_RESULT
        {
            EmscriptenDisplay* display = (EmscriptenDisplay*)user;
            int x = e->mouse.canvasX;
            int y = e->mouse.canvasY;
            int delta = (int)(e->deltaY * -60.0);
            display->mMouseWheel(display, x, y, delta);
            return true;
        });

    emscripten_set_keydown_callback(nullptr, this, true, 
        [](int, const EmscriptenKeyboardEvent* e, void* user) -> EMSCRIPTEN_RESULT
        {
            if (e->keyCode <= 0xff)
            {
                EmscriptenDisplay* display = (EmscriptenDisplay*)user;
                Key key = (Key)display->mKeyTable[e->keyCode];
                if (key != Key_None)
                {
                    EmscriptenDisplay* display = (EmscriptenDisplay*)user;
                    display->mKeyDown(display, key);
                    return true;
                }
            }
            return false;
        });

    emscripten_set_keyup_callback(nullptr, this, true, 
        [](int, const EmscriptenKeyboardEvent* e, void* user) -> EMSCRIPTEN_RESULT
        {
            if (e->keyCode <= 0xff)
            {
                EmscriptenDisplay* display = (EmscriptenDisplay*)user;
                Key key = (Key)display->mKeyTable[e->keyCode];
                if (key != Key_None)
                {
                    display->mKeyUp(display, key);
                    return true;
                }
            }
            return false;
        });

    emscripten_set_keypress_callback(nullptr, this, true,
        [](int, const EmscriptenKeyboardEvent* e, void* user) -> EMSCRIPTEN_RESULT
        {
            EmscriptenDisplay* display = (EmscriptenDisplay*)user;
            display->mChar(display, e->charCode);
            return true;
        });
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EmscriptenDisplay::EnterMessageLoop(bool /*runInBackground*/)
{
    emscripten_set_main_loop_arg([](void* arg)
        {
            ((EmscriptenDisplay*)arg)->mRender((EmscriptenDisplay*)arg);
        }, 
        this, 0, true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void* EmscriptenDisplay::GetNativeHandle() const
{
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t EmscriptenDisplay::GetClientWidth() const
{
    return mWidth;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t EmscriptenDisplay::GetClientHeight() const
{
    return mHeight;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_IMPLEMENT_REFLECTION(EmscriptenDisplay)
{
    NsMeta<TypeId>("EmscriptenDisplay");
    NsMeta<Category>("Display");
}
