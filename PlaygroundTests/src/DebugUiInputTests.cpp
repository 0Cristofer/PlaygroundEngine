#include <doctest/doctest.h>

import std;
import imgui;
import PlaygroundEngine.DebugUi;
import PlaygroundEngine.PlatformEvents;

namespace
{
	// The translation needs the record and nothing else, so these run with no window and no window
	// system: build a batch, submit it, open a frame, and ask ImGui what it believes.

	class ImGuiContextFixture
	{
	public:
		ImGuiContextFixture()
		{
			ImGui::CreateContext();

			ImGuiIO& io = ImGui::GetIO();
			io.DisplaySize = ImVec2(1280.0f, 720.0f);
			io.DeltaTime = 1.0f / 60.0f;

			// A font is never rasterized here, so ImGui is told the atlas is already usable rather
			// than left to ask a renderer backend that does not exist in this process.

			io.Fonts->AddFontDefaultBitmap();
			io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
		}

		~ImGuiContextFixture()
		{
			EndOpenFrame();

			ImGui::DestroyContext();
		}

		ImGuiContextFixture(const ImGuiContextFixture&) = delete;
		ImGuiContextFixture& operator=(const ImGuiContextFixture&) = delete;

		// The frame is left open on purpose. EndFrame clears the per-frame input, the wheel, typed
		// characters and the focus-loss flag among it, so asserting after closing would be asserting
		// on state ImGui has already wiped.

		void SubmitAndOpenFrame(const PgE::PlatformEventRecord& record)
		{
			EndOpenFrame();

			PgE::SubmitPlatformEvents(record);

			ImGui::NewFrame();
			_frameOpen = true;
		}

	private:
		void EndOpenFrame()
		{
			if (_frameOpen)
			{
				ImGui::EndFrame();
				_frameOpen = false;
			}
		}

		bool _frameOpen = false;
	};

	PgE::PlatformEventRecord RecordOf(const std::initializer_list<PgE::PlatformEvent> events)
	{
		PgE::PlatformEventRecord record;
		for (const PgE::PlatformEvent& event : events)
		{
			record.Append(event);
		}

		return record;
	}

	std::size_t CountKeysDown()
	{
		std::size_t downCount = 0;
		for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key)
		{
			// Mouse buttons and wheels occupy a contiguous block of the named range as aliases, and
			// are driven by the mouse events rather than the key path being counted here.

			if (key >= ImGuiKey_MouseLeft && key <= ImGuiKey_MouseWheelY)
			{
				continue;
			}

			if (ImGui::IsKeyDown(static_cast<ImGuiKey>(key)))
			{
				++downCount;
			}
		}

		return downCount;
	}
}

TEST_CASE("Every keyboard input code reaches ImGui as a key")
{
	ImGuiContextFixture context;

	// The two codes with no ImGui counterpart: World2 is the second of GLFW's non-US extras, and
	// ImGui's function keys stop at 24.

	static constexpr std::array unmapped = {PgE::InputCode::KeyWorld2, PgE::InputCode::KeyF25};

	for (std::uint16_t raw = static_cast<std::uint16_t>(PgE::InputCode::KeyboardRangeFirst);
		 raw <= static_cast<std::uint16_t>(PgE::InputCode::KeyboardRangeLast); ++raw)
	{
		const auto code = static_cast<PgE::InputCode>(raw);
		const bool expectedMapped = std::ranges::find(unmapped, code) == unmapped.end();

		context.SubmitAndOpenFrame(RecordOf({PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyPressed, .Code = code}}));

		INFO("input code ", raw);
		CHECK(CountKeysDown() == (expectedMapped ? 1u : 0u));

		context.SubmitAndOpenFrame(RecordOf({PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyReleased, .Code = code}}));

		CHECK(CountKeysDown() == 0u);
	}
}

TEST_CASE("Key presses land on the matching ImGui key")
{
	ImGuiContextFixture context;

	context.SubmitAndOpenFrame(RecordOf({PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyPressed, .Code = PgE::InputCode::KeyA}}));

	CHECK(ImGui::IsKeyDown(ImGuiKey_A));
	CHECK_FALSE(ImGui::IsKeyDown(ImGuiKey_B));

	context.SubmitAndOpenFrame(RecordOf({PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyReleased, .Code = PgE::InputCode::KeyA}}));

	CHECK_FALSE(ImGui::IsKeyDown(ImGuiKey_A));
}

TEST_CASE("Modifiers ride the event that carried them")
{
	ImGuiContextFixture context;

	context.SubmitAndOpenFrame(RecordOf({PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyPressed,
															.Code = PgE::InputCode::KeyS,
															.Modifiers = PgE::InputModifiers{.Shift = true, .Control = true}}}));

	const ImGuiIO& io = ImGui::GetIO();
	CHECK(io.KeyCtrl);
	CHECK(io.KeyShift);
	CHECK_FALSE(io.KeyAlt);
	CHECK_FALSE(io.KeySuper);
}

TEST_CASE("Pointer position, buttons and wheel reach ImGui")
{
	ImGuiContextFixture context;

	context.SubmitAndOpenFrame(
		RecordOf({PgE::PlatformEvent{.Type = PgE::PlatformEventType::PointerMoved, .X = 320.0f, .Y = 240.0f},
				  PgE::PlatformEvent{.Type = PgE::PlatformEventType::PointerButtonPressed, .Code = PgE::InputCode::PointerButtonRight}}));

	const ImGuiIO& io = ImGui::GetIO();

	CHECK(io.MousePos.x == doctest::Approx(320.0f));
	CHECK(io.MousePos.y == doctest::Approx(240.0f));
	CHECK(ImGui::IsMouseDown(ImGuiMouseButton_Right));
	CHECK_FALSE(ImGui::IsMouseDown(ImGuiMouseButton_Left));

	context.SubmitAndOpenFrame(RecordOf({PgE::PlatformEvent{.Type = PgE::PlatformEventType::PointerScrolled, .X = 0.0f, .Y = -2.0f}}));

	CHECK(io.MouseWheel == doctest::Approx(-2.0f));
}

TEST_CASE("Pointer codes past the second extra have no ImGui button")
{
	ImGuiContextFixture context;

	context.SubmitAndOpenFrame(
		RecordOf({PgE::PlatformEvent{.Type = PgE::PlatformEventType::PointerButtonPressed, .Code = PgE::InputCode::PointerButtonExtra3}}));

	for (int button = 0; button < ImGuiMouseButton_COUNT; ++button)
	{
		CHECK_FALSE(ImGui::IsMouseDown(static_cast<ImGuiMouseButton>(button)));
	}
}

TEST_CASE("Typed characters reach the input queue")
{
	ImGuiContextFixture context;

	context.SubmitAndOpenFrame(RecordOf({PgE::PlatformEvent{.Type = PgE::PlatformEventType::CharacterTyped, .Codepoint = U'g'}}));

	const ImGuiIO& io = ImGui::GetIO();

	REQUIRE(io.InputQueueCharacters.Size == 1);
	CHECK(io.InputQueueCharacters[0] == U'g');
}

TEST_CASE("Focus loss is forwarded")
{
	ImGuiContextFixture context;

	context.SubmitAndOpenFrame(RecordOf({PgE::PlatformEvent{.Type = PgE::PlatformEventType::KeyPressed, .Code = PgE::InputCode::KeyW}}));

	REQUIRE(CountKeysDown() == 1u);

	// Losing focus clears the held key, which is what stops a key held across an alt-tab from
	// sticking down forever.

	context.SubmitAndOpenFrame(RecordOf({PgE::PlatformEvent{.Type = PgE::PlatformEventType::FocusLost}}));

	CHECK(ImGui::GetIO().AppFocusLost);
	CHECK(CountKeysDown() == 0u);
}
