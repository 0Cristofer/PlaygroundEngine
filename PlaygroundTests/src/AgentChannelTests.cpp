#include <doctest/doctest.h>

// The channel does not exist in a release build, so neither does anything that tests it.

#if defined(PGE_DEV)

import std;
import PlaygroundEngine.AgentChannel;
import PlaygroundEngine.PlatformEvents;

namespace
{
	// The parser is the only part of the channel that is worth a test and cheap to write one for:
	// it is a pure string-to-command function, so it needs no socket, no thread, and no engine.

	const PgE::PlatformEvent* ParseEvent(const std::string_view line, std::expected<PgE::PlatformEvent, PgE::AgentCommandError>& storage)
	{
		storage = PgE::ParseCommand(line);

		if (!storage)
		{
			return nullptr;
		}

		return &*storage;
	}

	PgE::AgentCommandError ParseError(const std::string_view line)
	{
		const std::expected<PgE::PlatformEvent, PgE::AgentCommandError> command = PgE::ParseCommand(line);

		REQUIRE_FALSE(command.has_value());

		return command.error();
	}
}

TEST_CASE("Agent commands are named by the enumerators themselves")
{
	// The point of parsing through EnumFromName: the protocol is the enum. A verb that is not an
	// enumerator is not a command, and renaming an enumerator renames the command with it.

	std::expected<PgE::PlatformEvent, PgE::AgentCommandError> storage;

	const PgE::PlatformEvent* focusGained = ParseEvent("FocusGained", storage);

	REQUIRE(focusGained != nullptr);
	CHECK(focusGained->Type == PgE::PlatformEventType::FocusGained);

	CHECK(ParseError("focusgained") == PgE::AgentCommandError::UnknownCommand);
	CHECK(ParseError("focus_gained") == PgE::AgentCommandError::UnknownCommand);
	CHECK(ParseError("Nonsense") == PgE::AgentCommandError::UnknownCommand);
}

TEST_CASE("Agent pointer commands carry their coordinates")
{
	std::expected<PgE::PlatformEvent, PgE::AgentCommandError> storage;

	const PgE::PlatformEvent* moved = ParseEvent("PointerMoved 200 300.5", storage);

	REQUIRE(moved != nullptr);
	CHECK(moved->Type == PgE::PlatformEventType::PointerMoved);
	CHECK(moved->X == doctest::Approx(200.0f));
	CHECK(moved->Y == doctest::Approx(300.5f));

	const PgE::PlatformEvent* scrolled = ParseEvent("PointerScrolled 0 -2", storage);

	REQUIRE(scrolled != nullptr);
	CHECK(scrolled->Y == doctest::Approx(-2.0f));
}

TEST_CASE("Agent key commands resolve input codes and the repeat flag")
{
	std::expected<PgE::PlatformEvent, PgE::AgentCommandError> storage;

	const PgE::PlatformEvent* pressed = ParseEvent("KeyPressed KeyF12", storage);

	REQUIRE(pressed != nullptr);
	CHECK(pressed->Type == PgE::PlatformEventType::KeyPressed);
	CHECK(pressed->Code == PgE::InputCode::KeyF12);
	CHECK_FALSE(pressed->Repeat);

	const PgE::PlatformEvent* repeated = ParseEvent("KeyPressed KeyA repeat", storage);

	REQUIRE(repeated != nullptr);
	CHECK(repeated->Repeat);

	const PgE::PlatformEvent* button = ParseEvent("PointerButtonPressed PointerButtonLeft", storage);

	REQUIRE(button != nullptr);
	CHECK(button->Code == PgE::InputCode::PointerButtonLeft);
}

TEST_CASE("Agent commands reject codes from the wrong range")
{
	// A pointer button is a valid InputCode but not a valid key, so name resolution alone is not
	// enough: the code has to belong to the range the event type addresses.

	CHECK(ParseError("KeyPressed PointerButtonLeft") == PgE::AgentCommandError::BadArgument);
	CHECK(ParseError("PointerButtonPressed KeyA") == PgE::AgentCommandError::BadArgument);
	CHECK(ParseError("KeyPressed NotAKey") == PgE::AgentCommandError::BadArgument);
}

TEST_CASE("Agent commands reject malformed arguments")
{
	CHECK(ParseError("PointerMoved 200") == PgE::AgentCommandError::BadArgumentCount);
	CHECK(ParseError("PointerMoved 200 300 400") == PgE::AgentCommandError::BadArgumentCount);
	CHECK(ParseError("PointerMoved left right") == PgE::AgentCommandError::BadArgument);
	CHECK(ParseError("FocusGained 1") == PgE::AgentCommandError::BadArgumentCount);
	CHECK(ParseError("KeyReleased KeyA repeat") == PgE::AgentCommandError::BadArgument);
	CHECK(ParseError("") == PgE::AgentCommandError::UnknownCommand);

	// Injecting a size the window does not have would only force a redundant swapchain recreate.

	CHECK(ParseError("WindowResized 800 600") == PgE::AgentCommandError::UnknownCommand);
}

TEST_CASE("Agent pointer commands reject non-finite coordinates")
{
	// from_chars parses these happily. A NaN reaching the record poisons every consumer that
	// integrates pointer position, and does it silently, for the rest of the session.

	CHECK(ParseError("PointerMoved nan 0") == PgE::AgentCommandError::BadArgument);
	CHECK(ParseError("PointerMoved 0 inf") == PgE::AgentCommandError::BadArgument);
	CHECK(ParseError("PointerScrolled -inf 0") == PgE::AgentCommandError::BadArgument);
}

TEST_CASE("Agent channel knows only events, not a capture command")
{
	// There is one capture entry point, the F12 binding. A screenshot is that key injected, so the
	// channel deliberately has no verb of its own; scripts/pge expands it client-side.

	CHECK(ParseError("screenshot") == PgE::AgentCommandError::UnknownCommand);
	CHECK(ParseError("capture") == PgE::AgentCommandError::UnknownCommand);

	std::expected<PgE::PlatformEvent, PgE::AgentCommandError> storage;

	const PgE::PlatformEvent* captureKey = ParseEvent("KeyPressed KeyF12", storage);

	REQUIRE(captureKey != nullptr);
	CHECK(captureKey->Code == PgE::InputCode::KeyF12);
}

TEST_CASE("Agent command parsing tolerates surrounding whitespace")
{
	// The reader hands over whatever arrived before the newline, so trailing carriage returns and
	// padding are the client's habits rather than errors.

	std::expected<PgE::PlatformEvent, PgE::AgentCommandError> storage;

	const PgE::PlatformEvent* moved = ParseEvent("  PointerMoved   10   20  \r", storage);

	REQUIRE(moved != nullptr);
	CHECK(moved->X == doctest::Approx(10.0f));
	CHECK(moved->Y == doctest::Approx(20.0f));
}

#endif
