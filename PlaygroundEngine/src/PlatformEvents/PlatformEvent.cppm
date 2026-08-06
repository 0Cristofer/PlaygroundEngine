export module PlaygroundEngine.PlatformEvents:PlatformEvent;

import std;

import :InputCode;
import :InputLockState;
import :InputModifiers;
import :PlatformKeyToken;

namespace PgE
{
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

	/// A single event pushed by the underlying platform. Can come from different platform related systems.
	/// Flat POD deliberately: simple so upper layers can reinterpret it however they want. Unused fields per type
	/// are the price. We can revisit this in the future if ergonomics become a problem.
	export struct PlatformEvent
	{
		PlatformEventType Type;
		InputCode Code = InputCode::Unknown;
		InputModifiers Modifiers{};
		InputLockState Locks{};
		bool Repeat = false;
		PlatformKeyToken Token{};
		char32_t Codepoint = 0;

		// A position for PointerMoved, a delta for PointerMovedRelative and PointerScrolled, and the
		// new drawable size for WindowResized.

		float X = 0.0f;
		float Y = 0.0f;

		std::uint64_t Timestamp = 0;
	};
}
