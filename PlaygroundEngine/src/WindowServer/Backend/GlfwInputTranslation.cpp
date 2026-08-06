module;

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

module PlaygroundEngine.WindowServer;

import PlaygroundEngine.PlatformEvents;

import :GlfwInputTranslation;

namespace PgE
{
	InputCode TranslateGlfwKey(const int key)
	{
		switch (key)
		{
		case GLFW_KEY_A:
			return InputCode::KeyA;
		case GLFW_KEY_B:
			return InputCode::KeyB;
		case GLFW_KEY_C:
			return InputCode::KeyC;
		case GLFW_KEY_D:
			return InputCode::KeyD;
		case GLFW_KEY_E:
			return InputCode::KeyE;
		case GLFW_KEY_F:
			return InputCode::KeyF;
		case GLFW_KEY_G:
			return InputCode::KeyG;
		case GLFW_KEY_H:
			return InputCode::KeyH;
		case GLFW_KEY_I:
			return InputCode::KeyI;
		case GLFW_KEY_J:
			return InputCode::KeyJ;
		case GLFW_KEY_K:
			return InputCode::KeyK;
		case GLFW_KEY_L:
			return InputCode::KeyL;
		case GLFW_KEY_M:
			return InputCode::KeyM;
		case GLFW_KEY_N:
			return InputCode::KeyN;
		case GLFW_KEY_O:
			return InputCode::KeyO;
		case GLFW_KEY_P:
			return InputCode::KeyP;
		case GLFW_KEY_Q:
			return InputCode::KeyQ;
		case GLFW_KEY_R:
			return InputCode::KeyR;
		case GLFW_KEY_S:
			return InputCode::KeyS;
		case GLFW_KEY_T:
			return InputCode::KeyT;
		case GLFW_KEY_U:
			return InputCode::KeyU;
		case GLFW_KEY_V:
			return InputCode::KeyV;
		case GLFW_KEY_W:
			return InputCode::KeyW;
		case GLFW_KEY_X:
			return InputCode::KeyX;
		case GLFW_KEY_Y:
			return InputCode::KeyY;
		case GLFW_KEY_Z:
			return InputCode::KeyZ;

		case GLFW_KEY_0:
			return InputCode::Key0;
		case GLFW_KEY_1:
			return InputCode::Key1;
		case GLFW_KEY_2:
			return InputCode::Key2;
		case GLFW_KEY_3:
			return InputCode::Key3;
		case GLFW_KEY_4:
			return InputCode::Key4;
		case GLFW_KEY_5:
			return InputCode::Key5;
		case GLFW_KEY_6:
			return InputCode::Key6;
		case GLFW_KEY_7:
			return InputCode::Key7;
		case GLFW_KEY_8:
			return InputCode::Key8;
		case GLFW_KEY_9:
			return InputCode::Key9;

		case GLFW_KEY_SPACE:
			return InputCode::KeySpace;
		case GLFW_KEY_APOSTROPHE:
			return InputCode::KeyApostrophe;
		case GLFW_KEY_COMMA:
			return InputCode::KeyComma;
		case GLFW_KEY_MINUS:
			return InputCode::KeyMinus;
		case GLFW_KEY_PERIOD:
			return InputCode::KeyPeriod;
		case GLFW_KEY_SLASH:
			return InputCode::KeySlash;
		case GLFW_KEY_SEMICOLON:
			return InputCode::KeySemicolon;
		case GLFW_KEY_EQUAL:
			return InputCode::KeyEqual;
		case GLFW_KEY_LEFT_BRACKET:
			return InputCode::KeyLeftBracket;
		case GLFW_KEY_BACKSLASH:
			return InputCode::KeyBackslash;
		case GLFW_KEY_RIGHT_BRACKET:
			return InputCode::KeyRightBracket;
		case GLFW_KEY_GRAVE_ACCENT:
			return InputCode::KeyGraveAccent;
		case GLFW_KEY_WORLD_1:
			return InputCode::KeyWorld1;
		case GLFW_KEY_WORLD_2:
			return InputCode::KeyWorld2;

		case GLFW_KEY_ESCAPE:
			return InputCode::KeyEscape;
		case GLFW_KEY_ENTER:
			return InputCode::KeyEnter;
		case GLFW_KEY_TAB:
			return InputCode::KeyTab;
		case GLFW_KEY_BACKSPACE:
			return InputCode::KeyBackspace;
		case GLFW_KEY_INSERT:
			return InputCode::KeyInsert;
		case GLFW_KEY_DELETE:
			return InputCode::KeyDelete;
		case GLFW_KEY_RIGHT:
			return InputCode::KeyRight;
		case GLFW_KEY_LEFT:
			return InputCode::KeyLeft;
		case GLFW_KEY_DOWN:
			return InputCode::KeyDown;
		case GLFW_KEY_UP:
			return InputCode::KeyUp;
		case GLFW_KEY_PAGE_UP:
			return InputCode::KeyPageUp;
		case GLFW_KEY_PAGE_DOWN:
			return InputCode::KeyPageDown;
		case GLFW_KEY_HOME:
			return InputCode::KeyHome;
		case GLFW_KEY_END:
			return InputCode::KeyEnd;
		case GLFW_KEY_CAPS_LOCK:
			return InputCode::KeyCapsLock;
		case GLFW_KEY_SCROLL_LOCK:
			return InputCode::KeyScrollLock;
		case GLFW_KEY_NUM_LOCK:
			return InputCode::KeyNumLock;
		case GLFW_KEY_PRINT_SCREEN:
			return InputCode::KeyPrintScreen;
		case GLFW_KEY_PAUSE:
			return InputCode::KeyPause;
		case GLFW_KEY_MENU:
			return InputCode::KeyMenu;

		case GLFW_KEY_F1:
			return InputCode::KeyF1;
		case GLFW_KEY_F2:
			return InputCode::KeyF2;
		case GLFW_KEY_F3:
			return InputCode::KeyF3;
		case GLFW_KEY_F4:
			return InputCode::KeyF4;
		case GLFW_KEY_F5:
			return InputCode::KeyF5;
		case GLFW_KEY_F6:
			return InputCode::KeyF6;
		case GLFW_KEY_F7:
			return InputCode::KeyF7;
		case GLFW_KEY_F8:
			return InputCode::KeyF8;
		case GLFW_KEY_F9:
			return InputCode::KeyF9;
		case GLFW_KEY_F10:
			return InputCode::KeyF10;
		case GLFW_KEY_F11:
			return InputCode::KeyF11;
		case GLFW_KEY_F12:
			return InputCode::KeyF12;
		case GLFW_KEY_F13:
			return InputCode::KeyF13;
		case GLFW_KEY_F14:
			return InputCode::KeyF14;
		case GLFW_KEY_F15:
			return InputCode::KeyF15;
		case GLFW_KEY_F16:
			return InputCode::KeyF16;
		case GLFW_KEY_F17:
			return InputCode::KeyF17;
		case GLFW_KEY_F18:
			return InputCode::KeyF18;
		case GLFW_KEY_F19:
			return InputCode::KeyF19;
		case GLFW_KEY_F20:
			return InputCode::KeyF20;
		case GLFW_KEY_F21:
			return InputCode::KeyF21;
		case GLFW_KEY_F22:
			return InputCode::KeyF22;
		case GLFW_KEY_F23:
			return InputCode::KeyF23;
		case GLFW_KEY_F24:
			return InputCode::KeyF24;
		case GLFW_KEY_F25:
			return InputCode::KeyF25;

		case GLFW_KEY_KP_0:
			return InputCode::KeyNumpad0;
		case GLFW_KEY_KP_1:
			return InputCode::KeyNumpad1;
		case GLFW_KEY_KP_2:
			return InputCode::KeyNumpad2;
		case GLFW_KEY_KP_3:
			return InputCode::KeyNumpad3;
		case GLFW_KEY_KP_4:
			return InputCode::KeyNumpad4;
		case GLFW_KEY_KP_5:
			return InputCode::KeyNumpad5;
		case GLFW_KEY_KP_6:
			return InputCode::KeyNumpad6;
		case GLFW_KEY_KP_7:
			return InputCode::KeyNumpad7;
		case GLFW_KEY_KP_8:
			return InputCode::KeyNumpad8;
		case GLFW_KEY_KP_9:
			return InputCode::KeyNumpad9;
		case GLFW_KEY_KP_DECIMAL:
			return InputCode::KeyNumpadDecimal;
		case GLFW_KEY_KP_DIVIDE:
			return InputCode::KeyNumpadDivide;
		case GLFW_KEY_KP_MULTIPLY:
			return InputCode::KeyNumpadMultiply;
		case GLFW_KEY_KP_SUBTRACT:
			return InputCode::KeyNumpadSubtract;
		case GLFW_KEY_KP_ADD:
			return InputCode::KeyNumpadAdd;
		case GLFW_KEY_KP_ENTER:
			return InputCode::KeyNumpadEnter;
		case GLFW_KEY_KP_EQUAL:
			return InputCode::KeyNumpadEqual;

		case GLFW_KEY_LEFT_SHIFT:
			return InputCode::KeyLeftShift;
		case GLFW_KEY_LEFT_CONTROL:
			return InputCode::KeyLeftControl;
		case GLFW_KEY_LEFT_ALT:
			return InputCode::KeyLeftAlt;
		case GLFW_KEY_LEFT_SUPER:
			return InputCode::KeyLeftSuper;
		case GLFW_KEY_RIGHT_SHIFT:
			return InputCode::KeyRightShift;
		case GLFW_KEY_RIGHT_CONTROL:
			return InputCode::KeyRightControl;
		case GLFW_KEY_RIGHT_ALT:
			return InputCode::KeyRightAlt;
		case GLFW_KEY_RIGHT_SUPER:
			return InputCode::KeyRightSuper;

		default:
			return InputCode::Unknown;
		}
	}

	InputCode TranslateGlfwPointerButton(const int button)
	{
		switch (button)
		{
		case GLFW_MOUSE_BUTTON_LEFT:
			return InputCode::PointerButtonLeft;
		case GLFW_MOUSE_BUTTON_RIGHT:
			return InputCode::PointerButtonRight;
		case GLFW_MOUSE_BUTTON_MIDDLE:
			return InputCode::PointerButtonMiddle;
		case GLFW_MOUSE_BUTTON_4:
			return InputCode::PointerButtonExtra1;
		case GLFW_MOUSE_BUTTON_5:
			return InputCode::PointerButtonExtra2;
		case GLFW_MOUSE_BUTTON_6:
			return InputCode::PointerButtonExtra3;
		case GLFW_MOUSE_BUTTON_7:
			return InputCode::PointerButtonExtra4;
		case GLFW_MOUSE_BUTTON_8:
			return InputCode::PointerButtonExtra5;
		default:
			return InputCode::Unknown;
		}
	}

	InputModifiers TranslateGlfwModifiers(const int modifiers)
	{
		return InputModifiers{
			.Shift = (modifiers & GLFW_MOD_SHIFT) != 0,
			.Control = (modifiers & GLFW_MOD_CONTROL) != 0,
			.Alt = (modifiers & GLFW_MOD_ALT) != 0,
			.Super = (modifiers & GLFW_MOD_SUPER) != 0,
		};
	}

	// GLFW packs the lock state into the same bitfield as the held modifiers, and only reports it at
	// all because the backend enables GLFW_LOCK_KEY_MODS at window creation.

	InputLockState TranslateGlfwLockState(const int modifiers)
	{
		return InputLockState{
			.CapsLock = (modifiers & GLFW_MOD_CAPS_LOCK) != 0,
			.NumLock = (modifiers & GLFW_MOD_NUM_LOCK) != 0,
		};
	}
}
