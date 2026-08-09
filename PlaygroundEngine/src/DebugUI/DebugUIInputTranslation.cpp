module PlaygroundEngine.DebugUi;

import imgui;

import PlaygroundEngine.PlatformEvents;

import :InputTranslation;

namespace PgE
{
	// InputCode names a physical position and ImGuiKey names the key a layout would produce, so this
	// is exact only on a layout matching the positions. Correcting it needs the layout-dependent name
	// behind PlatformKeyToken, which only the window server can resolve.

	static ImGuiKey TranslateKeyCode(const InputCode code)
	{
		switch (code)
		{
		case InputCode::KeyA:
			return ImGuiKey_A;
		case InputCode::KeyB:
			return ImGuiKey_B;
		case InputCode::KeyC:
			return ImGuiKey_C;
		case InputCode::KeyD:
			return ImGuiKey_D;
		case InputCode::KeyE:
			return ImGuiKey_E;
		case InputCode::KeyF:
			return ImGuiKey_F;
		case InputCode::KeyG:
			return ImGuiKey_G;
		case InputCode::KeyH:
			return ImGuiKey_H;
		case InputCode::KeyI:
			return ImGuiKey_I;
		case InputCode::KeyJ:
			return ImGuiKey_J;
		case InputCode::KeyK:
			return ImGuiKey_K;
		case InputCode::KeyL:
			return ImGuiKey_L;
		case InputCode::KeyM:
			return ImGuiKey_M;
		case InputCode::KeyN:
			return ImGuiKey_N;
		case InputCode::KeyO:
			return ImGuiKey_O;
		case InputCode::KeyP:
			return ImGuiKey_P;
		case InputCode::KeyQ:
			return ImGuiKey_Q;
		case InputCode::KeyR:
			return ImGuiKey_R;
		case InputCode::KeyS:
			return ImGuiKey_S;
		case InputCode::KeyT:
			return ImGuiKey_T;
		case InputCode::KeyU:
			return ImGuiKey_U;
		case InputCode::KeyV:
			return ImGuiKey_V;
		case InputCode::KeyW:
			return ImGuiKey_W;
		case InputCode::KeyX:
			return ImGuiKey_X;
		case InputCode::KeyY:
			return ImGuiKey_Y;
		case InputCode::KeyZ:
			return ImGuiKey_Z;

		case InputCode::Key0:
			return ImGuiKey_0;
		case InputCode::Key1:
			return ImGuiKey_1;
		case InputCode::Key2:
			return ImGuiKey_2;
		case InputCode::Key3:
			return ImGuiKey_3;
		case InputCode::Key4:
			return ImGuiKey_4;
		case InputCode::Key5:
			return ImGuiKey_5;
		case InputCode::Key6:
			return ImGuiKey_6;
		case InputCode::Key7:
			return ImGuiKey_7;
		case InputCode::Key8:
			return ImGuiKey_8;
		case InputCode::Key9:
			return ImGuiKey_9;

		case InputCode::KeySpace:
			return ImGuiKey_Space;
		case InputCode::KeyApostrophe:
			return ImGuiKey_Apostrophe;
		case InputCode::KeyComma:
			return ImGuiKey_Comma;
		case InputCode::KeyMinus:
			return ImGuiKey_Minus;
		case InputCode::KeyPeriod:
			return ImGuiKey_Period;
		case InputCode::KeySlash:
			return ImGuiKey_Slash;
		case InputCode::KeySemicolon:
			return ImGuiKey_Semicolon;
		case InputCode::KeyEqual:
			return ImGuiKey_Equal;
		case InputCode::KeyLeftBracket:
			return ImGuiKey_LeftBracket;
		case InputCode::KeyBackslash:
			return ImGuiKey_Backslash;
		case InputCode::KeyRightBracket:
			return ImGuiKey_RightBracket;
		case InputCode::KeyGraveAccent:
			return ImGuiKey_GraveAccent;

			// The extra key an ISO layout carries and an ANSI one does not, which is what ImGui's OEM
			// slot names. World2 has no counterpart at all.

		case InputCode::KeyWorld1:
			return ImGuiKey_Oem102;

		case InputCode::KeyEscape:
			return ImGuiKey_Escape;
		case InputCode::KeyEnter:
			return ImGuiKey_Enter;
		case InputCode::KeyTab:
			return ImGuiKey_Tab;
		case InputCode::KeyBackspace:
			return ImGuiKey_Backspace;
		case InputCode::KeyInsert:
			return ImGuiKey_Insert;
		case InputCode::KeyDelete:
			return ImGuiKey_Delete;
		case InputCode::KeyRight:
			return ImGuiKey_RightArrow;
		case InputCode::KeyLeft:
			return ImGuiKey_LeftArrow;
		case InputCode::KeyDown:
			return ImGuiKey_DownArrow;
		case InputCode::KeyUp:
			return ImGuiKey_UpArrow;
		case InputCode::KeyPageUp:
			return ImGuiKey_PageUp;
		case InputCode::KeyPageDown:
			return ImGuiKey_PageDown;
		case InputCode::KeyHome:
			return ImGuiKey_Home;
		case InputCode::KeyEnd:
			return ImGuiKey_End;
		case InputCode::KeyCapsLock:
			return ImGuiKey_CapsLock;
		case InputCode::KeyScrollLock:
			return ImGuiKey_ScrollLock;
		case InputCode::KeyNumLock:
			return ImGuiKey_NumLock;
		case InputCode::KeyPrintScreen:
			return ImGuiKey_PrintScreen;
		case InputCode::KeyPause:
			return ImGuiKey_Pause;
		case InputCode::KeyMenu:
			return ImGuiKey_Menu;

		case InputCode::KeyF1:
			return ImGuiKey_F1;
		case InputCode::KeyF2:
			return ImGuiKey_F2;
		case InputCode::KeyF3:
			return ImGuiKey_F3;
		case InputCode::KeyF4:
			return ImGuiKey_F4;
		case InputCode::KeyF5:
			return ImGuiKey_F5;
		case InputCode::KeyF6:
			return ImGuiKey_F6;
		case InputCode::KeyF7:
			return ImGuiKey_F7;
		case InputCode::KeyF8:
			return ImGuiKey_F8;
		case InputCode::KeyF9:
			return ImGuiKey_F9;
		case InputCode::KeyF10:
			return ImGuiKey_F10;
		case InputCode::KeyF11:
			return ImGuiKey_F11;
		case InputCode::KeyF12:
			return ImGuiKey_F12;
		case InputCode::KeyF13:
			return ImGuiKey_F13;
		case InputCode::KeyF14:
			return ImGuiKey_F14;
		case InputCode::KeyF15:
			return ImGuiKey_F15;
		case InputCode::KeyF16:
			return ImGuiKey_F16;
		case InputCode::KeyF17:
			return ImGuiKey_F17;
		case InputCode::KeyF18:
			return ImGuiKey_F18;
		case InputCode::KeyF19:
			return ImGuiKey_F19;
		case InputCode::KeyF20:
			return ImGuiKey_F20;
		case InputCode::KeyF21:
			return ImGuiKey_F21;
		case InputCode::KeyF22:
			return ImGuiKey_F22;
		case InputCode::KeyF23:
			return ImGuiKey_F23;
		case InputCode::KeyF24:
			return ImGuiKey_F24;

		case InputCode::KeyNumpad0:
			return ImGuiKey_Keypad0;
		case InputCode::KeyNumpad1:
			return ImGuiKey_Keypad1;
		case InputCode::KeyNumpad2:
			return ImGuiKey_Keypad2;
		case InputCode::KeyNumpad3:
			return ImGuiKey_Keypad3;
		case InputCode::KeyNumpad4:
			return ImGuiKey_Keypad4;
		case InputCode::KeyNumpad5:
			return ImGuiKey_Keypad5;
		case InputCode::KeyNumpad6:
			return ImGuiKey_Keypad6;
		case InputCode::KeyNumpad7:
			return ImGuiKey_Keypad7;
		case InputCode::KeyNumpad8:
			return ImGuiKey_Keypad8;
		case InputCode::KeyNumpad9:
			return ImGuiKey_Keypad9;
		case InputCode::KeyNumpadDecimal:
			return ImGuiKey_KeypadDecimal;
		case InputCode::KeyNumpadDivide:
			return ImGuiKey_KeypadDivide;
		case InputCode::KeyNumpadMultiply:
			return ImGuiKey_KeypadMultiply;
		case InputCode::KeyNumpadSubtract:
			return ImGuiKey_KeypadSubtract;
		case InputCode::KeyNumpadAdd:
			return ImGuiKey_KeypadAdd;
		case InputCode::KeyNumpadEnter:
			return ImGuiKey_KeypadEnter;
		case InputCode::KeyNumpadEqual:
			return ImGuiKey_KeypadEqual;

		case InputCode::KeyLeftShift:
			return ImGuiKey_LeftShift;
		case InputCode::KeyLeftControl:
			return ImGuiKey_LeftCtrl;
		case InputCode::KeyLeftAlt:
			return ImGuiKey_LeftAlt;
		case InputCode::KeyLeftSuper:
			return ImGuiKey_LeftSuper;
		case InputCode::KeyRightShift:
			return ImGuiKey_RightShift;
		case InputCode::KeyRightControl:
			return ImGuiKey_RightCtrl;
		case InputCode::KeyRightAlt:
			return ImGuiKey_RightAlt;
		case InputCode::KeyRightSuper:
			return ImGuiKey_RightSuper;

		default:
			return ImGuiKey_None;
		}
	}

	// ImGui numbers its buttons rather than naming them, and only tracks ImGuiMouseButton_COUNT of
	// them, so the codes past the second extra have nowhere to go.

	static int TranslatePointerButton(const InputCode code)
	{
		switch (code)
		{
		case InputCode::PointerButtonLeft:
			return 0;
		case InputCode::PointerButtonRight:
			return 1;
		case InputCode::PointerButtonMiddle:
			return 2;
		case InputCode::PointerButtonExtra1:
			return 3;
		case InputCode::PointerButtonExtra2:
			return 4;
		default:
			return -1;
		}
	}

	// ImGui keeps modifiers in slots of their own, filled by the backend rather than derived from the
	// left/right key states. Every event carries the state it happened under, so this needs no
	// polling, and ImGui drops the repeats itself.

	static void SubmitKeyModifiers(ImGuiIO& io, const InputModifiers modifiers)
	{
		io.AddKeyEvent(ImGuiMod_Ctrl, modifiers.Control);
		io.AddKeyEvent(ImGuiMod_Shift, modifiers.Shift);
		io.AddKeyEvent(ImGuiMod_Alt, modifiers.Alt);
		io.AddKeyEvent(ImGuiMod_Super, modifiers.Super);
	}

	void SubmitPlatformEvents(const PlatformEventRecord& platformEventRecord)
	{
		ImGuiIO& io = ImGui::GetIO();

		for (const PlatformEvent& event : platformEventRecord.GetEvents())
		{
			switch (event.Type)
			{
			case PlatformEventType::KeyPressed:
			case PlatformEventType::KeyReleased: {
				if (const ImGuiKey key = TranslateKeyCode(event.Code); key != ImGuiKey_None)
				{
					SubmitKeyModifiers(io, event.Modifiers);
					io.AddKeyEvent(key, event.Type == PlatformEventType::KeyPressed);
				}

				break;
			}

			case PlatformEventType::CharacterTyped:
				io.AddInputCharacter(event.Codepoint);
				break;

			case PlatformEventType::PointerMoved:
				io.AddMousePosEvent(event.X, event.Y);
				break;

			case PlatformEventType::PointerButtonPressed:
			case PlatformEventType::PointerButtonReleased: {
				if (const int button = TranslatePointerButton(event.Code); button >= 0)
				{
					SubmitKeyModifiers(io, event.Modifiers);
					io.AddMouseButtonEvent(button, event.Type == PlatformEventType::PointerButtonPressed);
				}

				break;
			}

			case PlatformEventType::PointerScrolled:
				io.AddMouseWheelEvent(event.X, event.Y);
				break;

			case PlatformEventType::FocusGained:
				io.AddFocusEvent(true);
				break;

			case PlatformEventType::FocusLost:
				io.AddFocusEvent(false);
				break;

				// Relative pointer motion belongs to a captured cursor, which the debug UI never has;
				// the other two are answered by the sizes and lifetime the loop already passes in.

			case PlatformEventType::PointerMovedRelative:
			case PlatformEventType::WindowResized:
			case PlatformEventType::CloseRequested:
				break;
			}
		}
	}
}
