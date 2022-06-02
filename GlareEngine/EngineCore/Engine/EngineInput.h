#pragma once
namespace GlareEngine
{
	namespace EngineInput
	{
		void Initialize();
		void Shutdown();
		void Update(float FrameDelta);

		enum GInput
		{
			// keyboard
			// kKey must start at zero, see s_DXKeyMapping
			kKey_Escape = 0,
			kKey_1,
			kKey_2,
			kKey_3,
			kKey_4,
			kKey_5,
			kKey_6,
			kKey_7,
			kKey_8,
			kKey_9,
			kKey_0,
			kKey_Minus,
			kKey_Equals,
			kKey_Back,
			kKey_Tab,
			kKey_Q,
			kKey_W,
			kKey_E,
			kKey_R,
			kKey_T,
			kKey_Y,
			kKey_U,
			kKey_I,
			kKey_O,
			kKey_P,
			kKey_Lbracket,
			kKey_Rbracket,
			kKey_Return,
			kKey_Lcontrol,
			kKey_A,
			kKey_S,
			kKey_D,
			kKey_F,
			kKey_G,
			kKey_H,
			kKey_J,
			kKey_K,
			kKey_L,
			kKey_Semicolon,
			kKey_Apostrophe,
			kKey_Grave,
			kKey_Lshift,
			kKey_Backslash,
			kKey_Z,
			kKey_X,
			kKey_C,
			kKey_V,
			kKey_B,
			kKey_N,
			kKey_M,
			kKey_Comma,
			kKey_Period,
			kKey_Slash,
			kKey_Rshift,
			kKey_Multiply,
			kKey_Lalt,
			kKey_Space,
			kKey_Capital,
			kKey_F1,
			kKey_F2,
			kKey_F3,
			kKey_F4,
			kKey_F5,
			kKey_F6,
			kKey_F7,
			kKey_F8,
			kKey_F9,
			kKey_F10,
			kKey_Numlock,
			kKey_Scroll,
			kKey_Numpad7,
			kKey_Numpad8,
			kKey_Numpad9,
			kKey_Subtract,
			kKey_Numpad4,
			kKey_Numpad5,
			kKey_Numpad6,
			kKey_Add,
			kKey_Numpad1,
			kKey_Numpad2,
			kKey_Numpad3,
			kKey_Numpad0,
			kKey_Decimal,
			kKey_F11,
			kKey_F12,
			kKey_Numpadenter,
			kKey_Rcontrol,
			kKey_Divide,
			kKey_Sysrq,
			kKey_Ralt,
			kKey_Pause,
			kKey_Home,
			kKey_Up,
			kKey_Pgup,
			kKey_Left,
			kKey_Right,
			kKey_End,
			kKey_Down,
			kKey_Pgdn,
			kKey_Insert,
			kKey_Delete,
			kKey_Lwin,
			kKey_Rwin,
			kKey_Apps,

			kNumKeys,

			// Gamepad
			kDPadUp = kNumKeys,
			kDPadDown,
			kDPadLeft,
			kDPadRight,
			kStartButton,
			kBackButton,
			kLThumbClick,
			kRThumbClick,
			kLShoulder,
			kRShoulder,
			kAButton,
			kBButton,
			kXButton,
			kYButton,

			// mouse
			kMouse0,
			kMouse1,
			kMouse2,
			kMouse3,
			kMouse4,
			kMouse5,
			kMouse6,
			kMouse7,

			kNumDigitalInputs
		};


		bool IsAnyPressed(void);

		bool IsPressed(GInput di);
		bool IsFirstPressed(GInput di);
		bool IsReleased(GInput di);
		bool IsFirstReleased(GInput di);

		float GetDurationPressed(GInput di);
	};
}

