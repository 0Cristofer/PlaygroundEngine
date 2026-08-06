#include <doctest/doctest.h>

import std;
import PlaygroundEngine.PlatformEvents;

namespace
{
	// Everything above the platform layer is testable with no window and no window system, because
	// the root owns the record and the event is a plain POD. This is the intended shape of every
	// test for a future consumer: hand-build the batch, run the consumer, assert.

	PgE::PlatformEvent MakeKeyEvent(const PgE::PlatformEventType type, const PgE::InputCode code, const std::uint64_t timestamp)
	{
		return PgE::PlatformEvent{.Type = type, .Code = code, .Timestamp = timestamp};
	}
}

TEST_CASE("PlatformEventRecord hands out a non-destructive span")
{
	PgE::PlatformEventRecord record;

	CHECK(record.IsEmpty());

	record.Append(MakeKeyEvent(PgE::PlatformEventType::KeyPressed, PgE::InputCode::KeyW, 1));
	record.Append(MakeKeyEvent(PgE::PlatformEventType::KeyReleased, PgE::InputCode::KeyW, 2));

	const std::span<const PgE::PlatformEvent> firstReader = record.GetEvents();
	const std::span<const PgE::PlatformEvent> secondReader = record.GetEvents();

	CHECK(firstReader.size() == 2);
	CHECK(secondReader.size() == 2);
	CHECK(firstReader.data() == secondReader.data());
}

TEST_CASE("PlatformEventRecord preserves arrival order across event kinds")
{
	// The load-bearing property of the single record: a consumer must be able to tell that the
	// pointer moved before the button went down, and that a character arrived between two
	// editing keys.

	PgE::PlatformEventRecord record;

	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::PointerMoved, .X = 10.0f, .Y = 20.0f});
	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::PointerButtonPressed, .Code = PgE::InputCode::PointerButtonLeft});
	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::CharacterTyped, .Codepoint = U'a'});
	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyPressed, .Code = PgE::InputCode::KeyBackspace});
	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::CharacterTyped, .Codepoint = U'b'});

	const std::span<const PgE::PlatformEvent> events = record.GetEvents();

	REQUIRE(events.size() == 5);
	CHECK(events[0].Type == PgE::PlatformEventType::PointerMoved);
	CHECK(events[1].Type == PgE::PlatformEventType::PointerButtonPressed);
	CHECK(events[2].Codepoint == U'a');
	CHECK(events[3].Code == PgE::InputCode::KeyBackspace);
	CHECK(events[4].Codepoint == U'b');
}

TEST_CASE("A text consumer is a single forward pass over the record")
{
	// The pass the design specifies: characters insert, editing keys apply, every other key is
	// ignored because its effect already arrived as a character or there was none. Typing an
	// accent, e, a, Backspace must yield the composed character alone.

	PgE::PlatformEventRecord record;

	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyPressed, .Code = PgE::InputCode::KeyApostrophe});
	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyReleased, .Code = PgE::InputCode::KeyApostrophe});
	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyPressed, .Code = PgE::InputCode::KeyE});
	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::CharacterTyped, .Codepoint = U'é'});
	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyReleased, .Code = PgE::InputCode::KeyE});
	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyPressed, .Code = PgE::InputCode::KeyA});
	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::CharacterTyped, .Codepoint = U'a'});
	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyReleased, .Code = PgE::InputCode::KeyA});
	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyPressed, .Code = PgE::InputCode::KeyBackspace});
	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyReleased, .Code = PgE::InputCode::KeyBackspace});

	std::u32string text;

	for (const PgE::PlatformEvent& event : record.GetEvents())
	{
		if (event.Type == PgE::PlatformEventType::CharacterTyped)
		{
			text.push_back(event.Codepoint);
		}
		else if (event.Type == PgE::PlatformEventType::KeyPressed && event.Code == PgE::InputCode::KeyBackspace && !text.empty())
		{
			text.pop_back();
		}
	}

	CHECK(text == std::u32string{U'é'});
}

TEST_CASE("Held Backspace repeats reach a text consumer as separate deletions")
{
	// Repeat is carried and never filtered, because a text consumer must honor every repeat while
	// a gameplay consumer must honor only the first.

	PgE::PlatformEventRecord record;

	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyPressed, .Code = PgE::InputCode::KeyBackspace});
	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyPressed, .Code = PgE::InputCode::KeyBackspace, .Repeat = true});
	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyPressed, .Code = PgE::InputCode::KeyBackspace, .Repeat = true});

	std::u32string text = U"abcd";
	int gameplayActionCount = 0;

	for (const PgE::PlatformEvent& event : record.GetEvents())
	{
		if (event.Type != PgE::PlatformEventType::KeyPressed)
		{
			continue;
		}

		text.pop_back();

		if (!event.Repeat)
		{
			++gameplayActionCount;
		}
	}

	CHECK(text == std::u32string{U'a'});
	CHECK(gameplayActionCount == 1);
}

TEST_CASE("Clearing a record keeps its storage")
{
	PgE::PlatformEventRecord record;

	record.Append(MakeKeyEvent(PgE::PlatformEventType::KeyPressed, PgE::InputCode::KeyW, 1));

	const PgE::PlatformEvent* const storageBeforeClear = record.GetEvents().data();

	record.Clear();

	CHECK(record.IsEmpty());

	record.Append(MakeKeyEvent(PgE::PlatformEventType::KeyPressed, PgE::InputCode::KeyS, 2));

	CHECK(record.GetEvents().data() == storageBeforeClear);
}

TEST_CASE("InputCode ranges classify a code without a side table")
{
	CHECK(PgE::IsKeyboardCode(PgE::InputCode::KeyA));
	CHECK(PgE::IsKeyboardCode(PgE::InputCode::KeyRightSuper));
	CHECK_FALSE(PgE::IsKeyboardCode(PgE::InputCode::Unknown));
	CHECK_FALSE(PgE::IsKeyboardCode(PgE::InputCode::PointerButtonLeft));

	CHECK(PgE::IsPointerCode(PgE::InputCode::PointerButtonLeft));
	CHECK(PgE::IsPointerCode(PgE::InputCode::PointerButtonExtra5));
	CHECK_FALSE(PgE::IsPointerCode(PgE::InputCode::KeyA));

	// Nothing in the populated ranges is analog yet; the reservation is what keeps a binding table
	// from having to grow a side table when gamepad axes arrive.

	CHECK_FALSE(PgE::IsAnalogCode(PgE::InputCode::KeyA));
	CHECK_FALSE(PgE::IsAnalogCode(PgE::InputCode::PointerButtonLeft));

	static_assert(PgE::IsKeyboardCode(PgE::InputCode::KeyW));
	static_assert(!PgE::IsAnalogCode(PgE::InputCode::KeyW));
}

TEST_CASE("The keyboard, pointer and gamepad ranges do not overlap")
{
	for (std::uint16_t rawCode = 0; rawCode <= static_cast<std::uint16_t>(PgE::InputCode::AnalogRangeLast); ++rawCode)
	{
		const auto code = static_cast<PgE::InputCode>(rawCode);

		const int matchingRangeCount = static_cast<int>(PgE::IsKeyboardCode(code)) + static_cast<int>(PgE::IsPointerCode(code)) +
									   static_cast<int>(PgE::IsGamepadButtonCode(code)) + static_cast<int>(PgE::IsAnalogCode(code));

		REQUIRE(matchingRangeCount <= 1);
	}
}

TEST_CASE("Appending a batch preserves order and leaves earlier events alone")
{
	// The producer contract: a producer that cannot write straight into the record hands over what
	// it accumulated, and a second producer's batch lands after the first rather than replacing it.

	PgE::PlatformEventRecord record;
	PgE::PlatformEventRecord firstProducerBatch;
	PgE::PlatformEventRecord secondProducerBatch;

	firstProducerBatch.Append(MakeKeyEvent(PgE::PlatformEventType::KeyPressed, PgE::InputCode::KeyW, 1));
	firstProducerBatch.Append(MakeKeyEvent(PgE::PlatformEventType::KeyReleased, PgE::InputCode::KeyW, 2));
	secondProducerBatch.Append(MakeKeyEvent(PgE::PlatformEventType::KeyPressed, PgE::InputCode::KeyS, 3));

	record.Append(firstProducerBatch.GetEvents());
	record.Append(secondProducerBatch.GetEvents());

	const std::span<const PgE::PlatformEvent> events = record.GetEvents();

	REQUIRE(events.size() == 3);
	CHECK(events[0].Timestamp == 1);
	CHECK(events[1].Timestamp == 2);
	CHECK(events[2].Code == PgE::InputCode::KeyS);

	// Appending an empty batch is a no-op rather than a reset.

	record.Append(PgE::PlatformEventRecord{}.GetEvents());

	CHECK(record.GetEvents().size() == 3);
}

TEST_CASE("Lock state and held modifiers are independent facts")
{
	// A lock key is bindable as an ordinary key, which is why its press arrives as a KeyPressed with
	// its own InputCode. The latched state is separate, and a consumer reading one must not
	// accidentally read the other.

	PgE::PlatformEventRecord record;

	record.Append(PgE::PlatformEvent{
		.Type = PgE::PlatformEventType::KeyPressed, .Code = PgE::InputCode::KeyCapsLock, .Locks = PgE::InputLockState{.CapsLock = false}});
	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyPressed,
									 .Code = PgE::InputCode::KeyS,
									 .Modifiers = PgE::InputModifiers{.Control = true},
									 .Locks = PgE::InputLockState{.CapsLock = true}});

	int capsLockBindingActivations = 0;
	int saveBindingActivations = 0;

	for (const PgE::PlatformEvent& event : record.GetEvents())
	{
		if (event.Code == PgE::InputCode::KeyCapsLock)
		{
			++capsLockBindingActivations;
		}

		if (event.Code == PgE::InputCode::KeyS && event.Modifiers.Control)
		{
			++saveBindingActivations;
		}
	}

	CHECK(capsLockBindingActivations == 1);
	CHECK(saveBindingActivations == 1);

	CHECK_FALSE(record.GetEvents()[0].Locks.CapsLock);
	CHECK(record.GetEvents()[1].Locks.CapsLock);
}

TEST_CASE("HasEvent answers for the batch without exposing the payload")
{
	// What the root uses in place of a notification mechanism: the batch is already the data seam,
	// so noticing a close request is a query over it rather than a subscription.

	PgE::PlatformEventRecord record;

	CHECK_FALSE(record.HasEvent(PgE::PlatformEventType::CloseRequested));

	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::PointerMoved, .X = 1.0f, .Y = 2.0f});
	record.Append(PgE::PlatformEvent{.Type = PgE::PlatformEventType::CloseRequested});

	CHECK(record.HasEvent(PgE::PlatformEventType::CloseRequested));
	CHECK(record.HasEvent(PgE::PlatformEventType::PointerMoved));
	CHECK_FALSE(record.HasEvent(PgE::PlatformEventType::WindowResized));

	record.Clear();

	CHECK_FALSE(record.HasEvent(PgE::PlatformEventType::CloseRequested));
}

TEST_CASE("PlatformEvent stays a flat, trivially copyable POD")
{
	// Byte-level serialization, replay and a layout-matched C# struct all rest on this, so it is
	// pinned rather than left implied.

	static_assert(std::is_trivially_copyable_v<PgE::PlatformEvent>);
	static_assert(std::is_standard_layout_v<PgE::PlatformEvent>);
	static_assert(std::is_aggregate_v<PgE::PlatformEvent>);
}
