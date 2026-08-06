export module PlaygroundEngine.PlatformEvents:InputCode;

import std;

namespace PgE
{
	/// Represents all addressable input codes supported by the engine. One code space for every digital input, in ranges
	/// in the style of HID usage pages. Keyboard codes name physical positions, never the layout-dependent letter. The
	/// display label needs a connection query and is therefore a window server operation, not a property of the code.
	export enum class InputCode : std::uint16_t
	{
		Unknown = 0,

		KeyboardRangeFirst = 1,

		KeyA = KeyboardRangeFirst,
		KeyB,
		KeyC,
		KeyD,
		KeyE,
		KeyF,
		KeyG,
		KeyH,
		KeyI,
		KeyJ,
		KeyK,
		KeyL,
		KeyM,
		KeyN,
		KeyO,
		KeyP,
		KeyQ,
		KeyR,
		KeyS,
		KeyT,
		KeyU,
		KeyV,
		KeyW,
		KeyX,
		KeyY,
		KeyZ,

		Key0,
		Key1,
		Key2,
		Key3,
		Key4,
		Key5,
		Key6,
		Key7,
		Key8,
		Key9,

		KeySpace,
		KeyApostrophe,
		KeyComma,
		KeyMinus,
		KeyPeriod,
		KeySlash,
		KeySemicolon,
		KeyEqual,
		KeyLeftBracket,
		KeyBackslash,
		KeyRightBracket,
		KeyGraveAccent,
		KeyWorld1,
		KeyWorld2,

		KeyEscape,
		KeyEnter,
		KeyTab,
		KeyBackspace,
		KeyInsert,
		KeyDelete,
		KeyRight,
		KeyLeft,
		KeyDown,
		KeyUp,
		KeyPageUp,
		KeyPageDown,
		KeyHome,
		KeyEnd,
		KeyCapsLock,
		KeyScrollLock,
		KeyNumLock,
		KeyPrintScreen,
		KeyPause,
		KeyMenu,

		KeyF1,
		KeyF2,
		KeyF3,
		KeyF4,
		KeyF5,
		KeyF6,
		KeyF7,
		KeyF8,
		KeyF9,
		KeyF10,
		KeyF11,
		KeyF12,
		KeyF13,
		KeyF14,
		KeyF15,
		KeyF16,
		KeyF17,
		KeyF18,
		KeyF19,
		KeyF20,
		KeyF21,
		KeyF22,
		KeyF23,
		KeyF24,
		KeyF25,

		KeyNumpad0,
		KeyNumpad1,
		KeyNumpad2,
		KeyNumpad3,
		KeyNumpad4,
		KeyNumpad5,
		KeyNumpad6,
		KeyNumpad7,
		KeyNumpad8,
		KeyNumpad9,
		KeyNumpadDecimal,
		KeyNumpadDivide,
		KeyNumpadMultiply,
		KeyNumpadSubtract,
		KeyNumpadAdd,
		KeyNumpadEnter,
		KeyNumpadEqual,

		KeyLeftShift,
		KeyLeftControl,
		KeyLeftAlt,
		KeyLeftSuper,
		KeyRightShift,
		KeyRightControl,
		KeyRightAlt,
		KeyRightSuper,

		KeyboardRangeLast = KeyRightSuper,

		PointerRangeFirst = 256,

		PointerButtonLeft = PointerRangeFirst,
		PointerButtonRight,
		PointerButtonMiddle,
		PointerButtonExtra1,
		PointerButtonExtra2,
		PointerButtonExtra3,
		PointerButtonExtra4,
		PointerButtonExtra5,

		PointerRangeLast = PointerButtonExtra5,

		// Reserved for the device-input producer that will append to the same record.

		GamepadButtonRangeFirst = 512,
		GamepadButtonRangeLast = 639,

		AnalogRangeFirst = 640,
		AnalogRangeLast = 767,
	};

	export constexpr bool IsKeyboardCode(const InputCode code)
	{
		return code >= InputCode::KeyboardRangeFirst && code <= InputCode::KeyboardRangeLast;
	}

	export constexpr bool IsPointerCode(const InputCode code)
	{
		return code >= InputCode::PointerRangeFirst && code <= InputCode::PointerRangeLast;
	}

	export constexpr bool IsGamepadButtonCode(const InputCode code)
	{
		return code >= InputCode::GamepadButtonRangeFirst && code <= InputCode::GamepadButtonRangeLast;
	}

	export constexpr bool IsAnalogCode(const InputCode code)
	{
		return code >= InputCode::AnalogRangeFirst && code <= InputCode::AnalogRangeLast;
	}
}
