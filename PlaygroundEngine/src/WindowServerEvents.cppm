export module PlaygroundEngine.WindowServer:events;

import std;

namespace PgE
{
	// One code space for every digital input, in ranges in the style of HID usage pages. Split
	// enums per device would turn "Jump is Space, or Gamepad A, or Mouse 4" into a variant, and
	// every binding table, rebinding screen and serialized binding file would inherit it.

	// Keyboard codes name physical positions, never the layout-dependent letter, so movement
	// bindings stay under the same fingers on AZERTY. The display label needs a connection query
	// and is therefore a server operation, not a property of the code.

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

		// Reserved for the device-input session that will append to the same record. Empty on
		// purpose: the ranges are laid out now so adding gamepads later renumbers nothing that a
		// serialized binding file may already hold.

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

	// Analog-versus-digital is a property of the range rather than a side table, so it stays a
	// compile-time check. Nothing in the current ranges is analog; the binding layer needs the
	// distinction the moment gamepad axes arrive.

	export constexpr bool IsAnalogCode(const InputCode code)
	{
		return code >= InputCode::AnalogRangeFirst && code <= InputCode::AnalogRangeLast;
	}

	// Carried per event rather than derived from key transitions: with lock-key reporting enabled
	// the window system reports Caps Lock and Num Lock *lock state*, which no amount of watching
	// presses and releases reconstructs.

	export struct InputModifiers
	{
		bool Shift = false;
		bool Control = false;
		bool Alt = false;
		bool Super = false;
		bool CapsLock = false;
		bool NumLock = false;
	};

	// Opaque, meaningful only to the backend that produced it, and the only thing a layout-dependent
	// display name can be derived from.

	// TODO: decide its serialization before the first recording format exists. Either excluded from
	// the record on the wire, or written with a backend identifier and treated as advisory.

	export struct PlatformKeyToken
	{
		std::int32_t Value = 0;
	};

	export enum class PlatformEventType : std::uint8_t
	{
		KeyPressed,
		KeyReleased,
		CharacterTyped,

		PointerMoved,
		PointerMovedRelative,
		PointerButtonPressed,
		PointerButtonReleased,
		PointerScrolled,

		FocusGained,
		FocusLost,
		WindowResized,
		CloseRequested,
	};

	// Flat POD deliberately: contiguity, trivial copyability and byte-level serializability are what
	// make the record replayable, replicable and layout-matchable from C#. Unused fields per type
	// are the price; a union or a polymorphic hierarchy would cost all three.

	// X and Y carry a position for PointerMoved, a delta for PointerMovedRelative and
	// PointerScrolled, and the new drawable size for WindowResized. Button events carry no position:
	// the ordered PointerMoved before them is what says where the click landed.

	export struct PlatformEvent
	{
		PlatformEventType Type;
		InputCode Code = InputCode::Unknown;
		InputModifiers Modifiers{};
		bool Repeat = false;
		PlatformKeyToken Token{};
		char32_t Codepoint = 0;
		float X = 0.0f;
		float Y = 0.0f;
		std::uint64_t Timestamp = 0;
	};

	// Every event one pump delivered, in arrival order across kinds. That order is load-bearing: a
	// move before a press says where the click landed, a modifier before a click makes it a
	// shift-click, a character between two editing keys decides the resulting text.

	// A per-pump immutable batch. Pump() clears and fills it; afterwards it is read-only for the
	// frame and any number of consumers read the same span without draining it. Nothing survives
	// the next pump, so the layer that accumulates state must fold every pump unconditionally.

	export class PlatformEventRecord
	{
	public:
		static constexpr std::size_t DefaultReservedCapacity = 256;

		explicit PlatformEventRecord(const std::size_t reservedCapacity = DefaultReservedCapacity)
		{
			_events.reserve(reservedCapacity);
		}

		// Keeps the storage, so the steady state costs no allocation. A fixed ring is the wrong
		// shape: a window drag enters a nested modal loop and the pump after it delivers one
		// enormous batch.

		void Clear()
		{
			_events.clear();
		}

		void Append(const PlatformEvent& event)
		{
			_events.push_back(event);
		}

		[[nodiscard]] std::span<const PlatformEvent> GetEvents() const
		{
			return _events;
		}

		[[nodiscard]] bool IsEmpty() const
		{
			return _events.empty();
		}

	private:
		std::vector<PlatformEvent> _events;
	};
}
