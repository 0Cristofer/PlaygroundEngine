#include <doctest/doctest.h>

import std;
import imgui;
import PlaygroundEngine.DebugUi;
import PlaygroundEngine.Reflection;

namespace
{
	// The drawer emits ImGui calls and nothing else, so a context with no window and no renderer is
	// enough to walk every dispatch branch under enforced contracts: a bad pre, a null deref or a
	// misused facet aborts the run rather than merely looking wrong on screen.

	class ImGuiFrameFixture
	{
	public:
		ImGuiFrameFixture()
		{
			ImGui::CreateContext();

			ImGuiIO& io = ImGui::GetIO();
			io.DisplaySize = ImVec2(1280.0f, 720.0f);
			io.DeltaTime = 1.0f / 60.0f;
			io.Fonts->AddFontDefaultBitmap();
			io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

			ImGui::NewFrame();
			ImGui::Begin("DrawerTests");
		}

		~ImGuiFrameFixture()
		{
			ImGui::End();
			ImGui::EndFrame();
			ImGui::DestroyContext();
		}

		ImGuiFrameFixture(const ImGuiFrameFixture&) = delete;
		ImGuiFrameFixture& operator=(const ImGuiFrameFixture&) = delete;
	};

	enum class Facing : std::int8_t
	{
		Back = -1,
		Front [[maybe_unused]] = 1
	};

	struct Leaf
	{
		[[= PgE::DrawDebug{}]] float Value = 1.0f;
		[[maybe_unused]] int Hidden = 7;
	};

	struct Origin
	{
		[[= PgE::DrawDebug{}]] int Generation = 2;
		[[maybe_unused]] int HiddenInBase = 1;
	};

	// A base whose facet supersedes its structure: it publishes no fields, so a walk that only reads
	// GetFields() would draw an empty node where the string's own value belongs.
	struct Titled : std::string
	{
		[[= PgE::DrawDebug{}]] int Rank = 4;
	};

	struct Node : Origin
	{
		[[= PgE::DrawDebug{}]] bool Flag = true;
		[[= PgE::DrawDebug{}]] std::int32_t Count = 3;
		[[= PgE::DrawDebug{}]] Facing Direction = Facing::Back;
		[[= PgE::DrawDebug{}]] std::string Label = "leaf";
		[[= PgE::DrawDebug{}]] std::vector<float> Values = {1.0f, 2.0f};
		[[= PgE::DrawDebug{}]] Leaf Child;
		[[= PgE::DrawDebug{}]] Node* Self = nullptr;
		[[= PgE::DrawDebug{}]] const int Frozen = 9;
		[[= PgE::DrawDebug{}]] const Leaf* Reference = nullptr;

		// A const pointer to a mutable target: the row must not be read-only just because the pointer is.
		[[= PgE::DrawDebug{}]] [[maybe_unused]] Leaf* const Pinned = nullptr;

		// Shadows the base's annotated field: both rows are siblings in the panel, so they must not share
		// an ImGui id and the edit state that goes with it.
		[[= PgE::DrawDebug{}]] int Generation = 7;

		[[= PgE::DrawDebug{}]] Titled Heading;
		[[maybe_unused]] int Unannotated = 5;
	};
}

TEST_CASE("the panel drawer walks every supported field kind")
{
	const ImGuiFrameFixture frame;

	Node node;
	node.Self = &node;
	node.Reference = &node.Child;

	PgE::DebugPanelDrawer::Draw(node);

	// Drawing reads and never writes on its own: without input, a walk leaves the object alone.
	CHECK(node.Flag == true);
	CHECK(node.Count == 3);
	CHECK(node.Direction == Facing::Back);
	CHECK(node.Label == "leaf");
	CHECK(node.Values.size() == 2);
	CHECK(node.Child.Value == 1.0f);
	CHECK(node.Reference == &node.Child);

	// An inherited annotated field is reached by walking the bases; GetFields() is direct members only, so a
	// drawer that read only direct fields would silently omit it.
	CHECK(node.Origin::Generation == 2);
	CHECK(node.Generation == 7);

	// A facet-backed base draws through its facet rather than as an empty node.
	CHECK(node.Heading.Rank == 4);
}

TEST_CASE("a faceted root draws through its facet")
{
	const ImGuiFrameFixture frame;

	std::vector values{1.0f, 2.0f, 3.0f};

	// A facet supersedes the structural view, so a root that could only draw fields would dead-end here
	// on the very types the field path already handles.
	PgE::DebugPanelDrawer::Draw(values);

	CHECK(values.size() == 3);
}

TEST_CASE("the panel drawer accepts a const object")
{
	const ImGuiFrameFixture frame;

	const Node node;

	// A const object drives the same walk with every row read-only, which is the const_cast's bargain.
	PgE::DebugPanelDrawer::Draw(node);

	CHECK(node.Frozen == 9);
}

TEST_CASE("a self-referential pointer does not recurse until expanded")
{
	const ImGuiFrameFixture frame;

	Node node;
	node.Self = &node;

	// The cycle is walked only as deep as a reader opens it, so a closed panel terminates on its own.
	PgE::DebugPanelDrawer::Draw(node);

	CHECK(node.Self == &node);
}
