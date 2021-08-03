#include "EngineInput.h"
#include "EngineUtility.h"

#define USE_XINPUT
#include <XInput.h>
#pragma comment(lib, "xinput9_1_0.lib")

#define USE_KEYBOARD_MOUSE
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

using namespace GlareEngine;
namespace GlareEngine
{
	namespace GameCore
	{
		extern HWND g_hWnd;
	}
}

namespace
{
	bool s_Buttons[2][EngineInput::kNumDigitalInputs];
	float s_HoldDuration[EngineInput::kNumDigitalInputs] = { 0.0f };

#ifdef USE_KEYBOARD_MOUSE
	IDirectInput8A* s_DI;
	IDirectInputDevice8A* s_Keyboard;
	IDirectInputDevice8A* s_Mouse;
#endif


	DIMOUSESTATE2 s_MouseState;
	unsigned char s_Keybuffer[256];
	unsigned char s_DXKeyMapping[EngineInput::kNumKeys]; // map DigitalInput enum to DX key codes 


#ifdef USE_XINPUT
	float FilterAnalogInput(int val, int deadZone)
	{
		if (val < 0)
		{
			if (val > -deadZone)
				return 0.0f;
			else
				return (val + deadZone) / (32768.0f - deadZone);
		}
		else
		{
			if (val < deadZone)
				return 0.0f;
			else
				return (val - deadZone) / (32767.0f - deadZone);
		}
	}
#endif

#ifdef USE_KEYBOARD_MOUSE
	void BuildKeyMapping()
	{
		s_DXKeyMapping[EngineInput::kKey_Escape] = 1;
		s_DXKeyMapping[EngineInput::kKey_1] = 2;
		s_DXKeyMapping[EngineInput::kKey_2] = 3;
		s_DXKeyMapping[EngineInput::kKey_3] = 4;
		s_DXKeyMapping[EngineInput::kKey_4] = 5;
		s_DXKeyMapping[EngineInput::kKey_5] = 6;
		s_DXKeyMapping[EngineInput::kKey_6] = 7;
		s_DXKeyMapping[EngineInput::kKey_7] = 8;
		s_DXKeyMapping[EngineInput::kKey_8] = 9;
		s_DXKeyMapping[EngineInput::kKey_9] = 10;
		s_DXKeyMapping[EngineInput::kKey_0] = 11;
		s_DXKeyMapping[EngineInput::kKey_Minus] = 12;
		s_DXKeyMapping[EngineInput::kKey_Equals] = 13;
		s_DXKeyMapping[EngineInput::kKey_Back] = 14;
		s_DXKeyMapping[EngineInput::kKey_Tab] = 15;
		s_DXKeyMapping[EngineInput::kKey_Q] = 16;
		s_DXKeyMapping[EngineInput::kKey_W] = 17;
		s_DXKeyMapping[EngineInput::kKey_E] = 18;
		s_DXKeyMapping[EngineInput::kKey_R] = 19;
		s_DXKeyMapping[EngineInput::kKey_T] = 20;
		s_DXKeyMapping[EngineInput::kKey_Y] = 21;
		s_DXKeyMapping[EngineInput::kKey_U] = 22;
		s_DXKeyMapping[EngineInput::kKey_I] = 23;
		s_DXKeyMapping[EngineInput::kKey_O] = 24;
		s_DXKeyMapping[EngineInput::kKey_P] = 25;
		s_DXKeyMapping[EngineInput::kKey_Lbracket] = 26;
		s_DXKeyMapping[EngineInput::kKey_Rbracket] = 27;
		s_DXKeyMapping[EngineInput::kKey_Return] = 28;
		s_DXKeyMapping[EngineInput::kKey_Lcontrol] = 29;
		s_DXKeyMapping[EngineInput::kKey_A] = 30;
		s_DXKeyMapping[EngineInput::kKey_S] = 31;
		s_DXKeyMapping[EngineInput::kKey_D] = 32;
		s_DXKeyMapping[EngineInput::kKey_F] = 33;
		s_DXKeyMapping[EngineInput::kKey_G] = 34;
		s_DXKeyMapping[EngineInput::kKey_H] = 35;
		s_DXKeyMapping[EngineInput::kKey_J] = 36;
		s_DXKeyMapping[EngineInput::kKey_K] = 37;
		s_DXKeyMapping[EngineInput::kKey_L] = 38;
		s_DXKeyMapping[EngineInput::kKey_Semicolon] = 39;
		s_DXKeyMapping[EngineInput::kKey_Apostrophe] = 40;
		s_DXKeyMapping[EngineInput::kKey_Grave] = 41;
		s_DXKeyMapping[EngineInput::kKey_Lshift] = 42;
		s_DXKeyMapping[EngineInput::kKey_Backslash] = 43;
		s_DXKeyMapping[EngineInput::kKey_Z] = 44;
		s_DXKeyMapping[EngineInput::kKey_X] = 45;
		s_DXKeyMapping[EngineInput::kKey_C] = 46;
		s_DXKeyMapping[EngineInput::kKey_V] = 47;
		s_DXKeyMapping[EngineInput::kKey_B] = 48;
		s_DXKeyMapping[EngineInput::kKey_N] = 49;
		s_DXKeyMapping[EngineInput::kKey_M] = 50;
		s_DXKeyMapping[EngineInput::kKey_Comma] = 51;
		s_DXKeyMapping[EngineInput::kKey_Period] = 52;
		s_DXKeyMapping[EngineInput::kKey_Slash] = 53;
		s_DXKeyMapping[EngineInput::kKey_Rshift] = 54;
		s_DXKeyMapping[EngineInput::kKey_Multiply] = 55;
		s_DXKeyMapping[EngineInput::kKey_Lalt] = 56;
		s_DXKeyMapping[EngineInput::kKey_Space] = 57;
		s_DXKeyMapping[EngineInput::kKey_Capital] = 58;
		s_DXKeyMapping[EngineInput::kKey_F1] = 59;
		s_DXKeyMapping[EngineInput::kKey_F2] = 60;
		s_DXKeyMapping[EngineInput::kKey_F3] = 61;
		s_DXKeyMapping[EngineInput::kKey_F4] = 62;
		s_DXKeyMapping[EngineInput::kKey_F5] = 63;
		s_DXKeyMapping[EngineInput::kKey_F6] = 64;
		s_DXKeyMapping[EngineInput::kKey_F7] = 65;
		s_DXKeyMapping[EngineInput::kKey_F8] = 66;
		s_DXKeyMapping[EngineInput::kKey_F9] = 67;
		s_DXKeyMapping[EngineInput::kKey_F10] = 68;
		s_DXKeyMapping[EngineInput::kKey_Numlock] = 69;
		s_DXKeyMapping[EngineInput::kKey_Scroll] = 70;
		s_DXKeyMapping[EngineInput::kKey_Numpad7] = 71;
		s_DXKeyMapping[EngineInput::kKey_Numpad8] = 72;
		s_DXKeyMapping[EngineInput::kKey_Numpad9] = 73;
		s_DXKeyMapping[EngineInput::kKey_Subtract] = 74;
		s_DXKeyMapping[EngineInput::kKey_Numpad4] = 75;
		s_DXKeyMapping[EngineInput::kKey_Numpad5] = 76;
		s_DXKeyMapping[EngineInput::kKey_Numpad6] = 77;
		s_DXKeyMapping[EngineInput::kKey_Add] = 78;
		s_DXKeyMapping[EngineInput::kKey_Numpad1] = 79;
		s_DXKeyMapping[EngineInput::kKey_Numpad2] = 80;
		s_DXKeyMapping[EngineInput::kKey_Numpad3] = 81;
		s_DXKeyMapping[EngineInput::kKey_Numpad0] = 82;
		s_DXKeyMapping[EngineInput::kKey_Decimal] = 83;
		s_DXKeyMapping[EngineInput::kKey_F11] = 87;
		s_DXKeyMapping[EngineInput::kKey_F12] = 88;
		s_DXKeyMapping[EngineInput::kKey_Numpadenter] = 156;
		s_DXKeyMapping[EngineInput::kKey_Rcontrol] = 157;
		s_DXKeyMapping[EngineInput::kKey_Divide] = 181;
		s_DXKeyMapping[EngineInput::kKey_Sysrq] = 183;
		s_DXKeyMapping[EngineInput::kKey_Ralt] = 184;
		s_DXKeyMapping[EngineInput::kKey_Pause] = 197;
		s_DXKeyMapping[EngineInput::kKey_Home] = 199;
		s_DXKeyMapping[EngineInput::kKey_Up] = 200;
		s_DXKeyMapping[EngineInput::kKey_Pgup] = 201;
		s_DXKeyMapping[EngineInput::kKey_Left] = 203;
		s_DXKeyMapping[EngineInput::kKey_Right] = 205;
		s_DXKeyMapping[EngineInput::kKey_End] = 207;
		s_DXKeyMapping[EngineInput::kKey_Down] = 208;
		s_DXKeyMapping[EngineInput::kKey_Pgdn] = 209;
		s_DXKeyMapping[EngineInput::kKey_Insert] = 210;
		s_DXKeyMapping[EngineInput::kKey_Delete] = 211;
		s_DXKeyMapping[EngineInput::kKey_Lwin] = 219;
		s_DXKeyMapping[EngineInput::kKey_Rwin] = 220;
		s_DXKeyMapping[EngineInput::kKey_Apps] = 221;
	}

	void ZeroInputs()
	{
		memset(&s_MouseState, 0, sizeof(DIMOUSESTATE2));
		memset(s_Keybuffer, 0, sizeof(s_Keybuffer));
	}

	void KMInitialize()
	{
		BuildKeyMapping();

		if (FAILED(DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&s_DI, nullptr)))
			//DirectInput8 initialization failed.
			assert(false);
		if (FAILED(s_DI->CreateDevice(GUID_SysKeyboard, &s_Keyboard, nullptr)))
			//Keyboard CreateDevice failed.
			assert(false);
		if (FAILED(s_Keyboard->SetDataFormat(&c_dfDIKeyboard)))
			//Keyboard SetDataFormat failed.
			assert(false);
		if (FAILED(s_Keyboard->SetCooperativeLevel(GameCore::g_hWnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE)))
			//Keyboard SetCooperativeLevel failed.
			assert(false);

		DIPROPDWORD dipdw;
		dipdw.diph.dwSize = sizeof(DIPROPDWORD);
		dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		dipdw.diph.dwObj = 0;
		dipdw.diph.dwHow = DIPH_DEVICE;
		dipdw.dwData = 10;
		if (FAILED(s_Keyboard->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph)))
			//Keyboard set buffer size failed.
			assert(false);

		if (FAILED(s_DI->CreateDevice(GUID_SysMouse, &s_Mouse, nullptr)))
			//Mouse CreateDevice failed.
			assert(false);
		if (FAILED(s_Mouse->SetDataFormat(&c_dfDIMouse2)))
			//Mouse SetDataFormat failed.
			assert(false);
		if (FAILED(s_Mouse->SetCooperativeLevel(GameCore::g_hWnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE)))
			//Mouse SetCooperativeLevel failed.
			assert(false);

		ZeroInputs();
	}

	void KMShutdown()
	{
		if (s_Keyboard)
		{
			s_Keyboard->Unacquire();
			s_Keyboard->Release();
			s_Keyboard = nullptr;
		}
		if (s_Mouse)
		{
			s_Mouse->Unacquire();
			s_Mouse->Release();
			s_Mouse = nullptr;
		}
		if (s_DI)
		{
			s_DI->Release();
			s_DI = nullptr;
		}
	}

	void KMUpdate()
	{
		HWND foreground = GetForegroundWindow();
		bool visible = IsWindowVisible(foreground) != 0;

		if (foreground != GameCore::g_hWnd // wouldn't be able to acquire
			|| !visible)
		{
			ZeroInputs();
		}
		else
		{
			s_Mouse->Acquire();
			s_Mouse->GetDeviceState(sizeof(DIMOUSESTATE2), &s_MouseState);
			s_Keyboard->Acquire();
			s_Keyboard->GetDeviceState(sizeof(s_Keybuffer), s_Keybuffer);
			
		}

	}


#endif



}

void EngineInput::Initialize()
{
	ZeroMemory(s_Buttons, sizeof(s_Buttons));

#ifdef USE_KEYBOARD_MOUSE
	KMInitialize();
#endif
}

void EngineInput::Shutdown()
{
#ifdef USE_KEYBOARD_MOUSE
	KMShutdown();
#endif
}

void EngineInput::Update(float FrameDelta)
{
	memcpy(s_Buttons[1], s_Buttons[0], sizeof(s_Buttons[0]));
	memset(s_Buttons[0], 0, sizeof(s_Buttons[0]));

#ifdef USE_XINPUT
	XINPUT_STATE newInputState;
	if (ERROR_SUCCESS == XInputGetState(0, &newInputState))
	{
		if (newInputState.Gamepad.wButtons & (1 << 0)) s_Buttons[0][kDPadUp] = true;
		if (newInputState.Gamepad.wButtons & (1 << 1)) s_Buttons[0][kDPadDown] = true;
		if (newInputState.Gamepad.wButtons & (1 << 2)) s_Buttons[0][kDPadLeft] = true;
		if (newInputState.Gamepad.wButtons & (1 << 3)) s_Buttons[0][kDPadRight] = true;
		if (newInputState.Gamepad.wButtons & (1 << 4)) s_Buttons[0][kStartButton] = true;
		if (newInputState.Gamepad.wButtons & (1 << 5)) s_Buttons[0][kBackButton] = true;
		if (newInputState.Gamepad.wButtons & (1 << 6)) s_Buttons[0][kLThumbClick] = true;
		if (newInputState.Gamepad.wButtons & (1 << 7)) s_Buttons[0][kRThumbClick] = true;
		if (newInputState.Gamepad.wButtons & (1 << 8)) s_Buttons[0][kLShoulder] = true;
		if (newInputState.Gamepad.wButtons & (1 << 9)) s_Buttons[0][kRShoulder] = true;
		if (newInputState.Gamepad.wButtons & (1 << 12)) s_Buttons[0][kAButton] = true;
		if (newInputState.Gamepad.wButtons & (1 << 13)) s_Buttons[0][kBButton] = true;
		if (newInputState.Gamepad.wButtons & (1 << 14)) s_Buttons[0][kXButton] = true;
		if (newInputState.Gamepad.wButtons & (1 << 15)) s_Buttons[0][kYButton] = true;

	}
#endif

#ifdef USE_KEYBOARD_MOUSE
	KMUpdate();

	for (uint32_t i = 0; i < kNumKeys; ++i)
	{
		s_Buttons[0][i] = (s_Keybuffer[s_DXKeyMapping[i]] & 0x80) != 0;
	}

	for (uint32_t i = 0; i < 8; ++i)
	{
		if (s_MouseState.rgbButtons[i] > 0) s_Buttons[0][kMouse0 + i] = true;
	}

#endif

	// Update time duration for buttons pressed
	for (uint32_t i = 0; i < kNumDigitalInputs; ++i)
	{
		if (s_Buttons[0][i])
		{
			if (!s_Buttons[1][i])
				s_HoldDuration[i] = 0.0f;
			else
				s_HoldDuration[i] += FrameDelta;
		}
	}
}

bool EngineInput::IsAnyPressed(void)
{
	return s_Buttons[0] != 0;
}

bool EngineInput::IsPressed(GInput di)
{
	return s_Buttons[0][di];
}

bool EngineInput::IsFirstPressed(GInput di)
{
	return s_Buttons[0][di] && !s_Buttons[1][di];
}

bool EngineInput::IsReleased(GInput di)
{
	return !s_Buttons[0][di];
}

bool EngineInput::IsFirstReleased(GInput di)
{
	return !s_Buttons[0][di] && s_Buttons[1][di];
}

float GlareEngine::EngineInput::GetDurationPressed(GInput di)
{
	return s_HoldDuration[di];
}
