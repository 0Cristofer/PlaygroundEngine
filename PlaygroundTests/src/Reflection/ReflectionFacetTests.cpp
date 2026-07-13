#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Reflection;
import PlaygroundTests.ReflectionTestTypes;

using namespace ReflectionTestTypes;

TEST_CASE("the enumeration facet lives in the generic facet table")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Permissions>();

	// GetEnumeration is now sugar over GetFacet<EnumerationFacet>; both resolve to the one stored entry.
	REQUIRE(type.GetFacets().size() == 1);
	CHECK(type.GetFacet<PgE::EnumerationFacet>() == type.GetFacet<PgE::EnumerationFacet>());
	CHECK(type.GetFacet<PgE::EnumerationFacet>() != nullptr);

	// The entry's key resolves to EnumerationFacet's own TypeInfo, the pointer identity GetFacet matches on.
	CHECK(&type.GetFacets().front().Key.Get() == &PgE::TypeOf<PgE::EnumerationFacet>());

	// A type with no facet has an empty table, and a facet query on it is null, not a crash.
	CHECK(PgE::TypeOf<Widget>().GetFacets().empty());
	CHECK(PgE::TypeOf<Widget>().GetFacet<PgE::EnumerationFacet>() == nullptr);
}

TEST_CASE("string facet reads and writes through the erased view")
{
	const PgE::StringFacet* facet = PgE::TypeOf<std::string>().GetFacet<PgE::StringFacet>();
	REQUIRE(facet != nullptr);
	REQUIRE(facet->CanAssign());

	std::string value = "hello";
	CHECK(facet->View(&value) == "hello");

	REQUIRE(facet->Assign(&value, "world").has_value());
	CHECK(value == "world");
}

TEST_CASE("string_view facet reads but has no assign")
{
	const PgE::StringFacet* facet = PgE::TypeOf<std::string_view>().GetFacet<PgE::StringFacet>();
	REQUIRE(facet != nullptr);
	CHECK_FALSE(facet->CanAssign());

	constexpr std::string_view view = "borrowed";
	CHECK(facet->View(&view) == "borrowed");

	// The absent write half reports NotWritable rather than crashing on a null thunk.
	std::string_view target = view;
	const auto assigned = facet->Assign(&target, "other");
	REQUIRE_FALSE(assigned.has_value());
	CHECK(assigned.error().Reason == PgE::FacetError::NotWritable);
}

TEST_CASE("a std container reflects as its facet, not its structural fields")
{
	// The recursion firewall: a superseding facet gives empty field and function spans, so reflecting a
	// std container stops at the protocol boundary rather than walking libstdc++ internals.
	const PgE::TypeInfo& vectorType = PgE::TypeOf<std::vector<int>>();
	REQUIRE(vectorType.GetFacet<PgE::SequenceFacet>() != nullptr);
	CHECK(vectorType.GetFunctions().empty());
	CHECK(vectorType.FindFieldByIdentifier("Slots") == nullptr);

	CHECK(PgE::TypeOf<std::string>().GetFunctions().empty());
}

TEST_CASE("sequence facet walks and mutates a vector's elements")
{
	std::vector<int> numbers{10, 20, 30};
	const PgE::SequenceFacet* facet = PgE::TypeOf<std::vector<int>>().GetFacet<PgE::SequenceFacet>();
	REQUIRE(facet != nullptr);

	CHECK(&facet->ElementType() == &PgE::TypeOf<int>());
	CHECK(facet->Size(&numbers) == 3);

	// A const object selects the const element-ref overload (as ObjectToString does with its const void*).
	const std::vector<int>& constNumbers = numbers;
	const PgE::TypedRef first = facet->ElementRef(&constNumbers, 0);
	CHECK(first.Type == &PgE::TypeOf<int>());
	CHECK(first.IsConst);
	CHECK(*static_cast<const int*>(first.Data) == 10);

	const PgE::TypedRef second = facet->ElementRef(&numbers, 1);
	CHECK_FALSE(second.IsConst);
	*static_cast<int*>(second.Data) = 99;
	CHECK(numbers[1] == 99);
}

TEST_CASE("sequence facet rebuilds a vector via clear, reserve, and append")
{
	std::vector<int> numbers{1, 2, 3};
	const PgE::SequenceFacet* facet = PgE::TypeOf<std::vector<int>>().GetFacet<PgE::SequenceFacet>();
	REQUIRE(facet != nullptr);

	REQUIRE(facet->Clear(&numbers).has_value());
	CHECK(numbers.empty());
	REQUIRE(facet->Reserve(&numbers, 2).has_value());

	int a = 7;
	int b = 8;
	REQUIRE(facet->Append(&numbers, {.Type = &PgE::TypeOf<int>(), .Data = &a, .IsConst = true}).has_value());
	REQUIRE(facet->Append(&numbers, {.Type = &PgE::TypeOf<int>(), .Data = &b, .IsConst = true}).has_value());
	REQUIRE(numbers.size() == 2);
	CHECK(numbers[0] == 7);
	CHECK(numbers[1] == 8);

	// A mistyped element is rejected before it touches the container.
	std::string wrong = "x";
	const auto bad = facet->Append(&numbers, {.Type = &PgE::TypeOf<std::string>(), .Data = &wrong, .IsConst = true});
	REQUIRE_FALSE(bad.has_value());
	CHECK(bad.error().Reason == PgE::FacetError::TypeMismatch);
	CHECK(numbers.size() == 2);
}

TEST_CASE("sequence facet appends a move-only element by move, not by copy")
{
	std::vector<MoveOnlyValue> items;
	const PgE::SequenceFacet* facet = PgE::TypeOf<std::vector<MoveOnlyValue>>().GetFacet<PgE::SequenceFacet>();
	REQUIRE(facet != nullptr);

	MoveOnlyValue source;
	source.Tag = 5;

	// A copy request cannot append a move-only element (mirrors the field-set move protocol).
	const auto copied = facet->Append(&items, {.Type = &PgE::TypeOf<MoveOnlyValue>(), .Data = &source});
	REQUIRE_FALSE(copied.has_value());
	CHECK(copied.error().Reason == PgE::FacetError::NotWritable);

	// A move from a mutable source succeeds.
	const auto moved = facet->Append(&items, {.Type = &PgE::TypeOf<MoveOnlyValue>(), .Data = &source, .IsConst = false, .Movable = true});
	REQUIRE(moved.has_value());
	REQUIRE(items.size() == 1);
	CHECK(items[0].Tag == 5);
}

TEST_CASE("fixed-size sequences expose elements but no resize thunks")
{
	const PgE::SequenceFacet* arrayFacet = PgE::TypeOf<std::array<int, 3>>().GetFacet<PgE::SequenceFacet>();
	REQUIRE(arrayFacet != nullptr);
	CHECK_FALSE(arrayFacet->CanClear());
	CHECK_FALSE(arrayFacet->CanReserve());
	CHECK_FALSE(arrayFacet->CanAppend());

	std::array<int, 3> values{1, 2, 3};
	CHECK(arrayFacet->Size(&values) == 3);
	const PgE::TypedRef ref = arrayFacet->ElementRef(&values, 2);
	*static_cast<int*>(ref.Data) = 42;
	CHECK(values[2] == 42);

	const PgE::SequenceFacet* cArrayFacet = PgE::TypeOf<int[4]>().GetFacet<PgE::SequenceFacet>();
	REQUIRE(cArrayFacet != nullptr);
	CHECK_FALSE(cArrayFacet->CanAppend());
	CHECK(&cArrayFacet->ElementType() == &PgE::TypeOf<int>());

	const int raw[4] = {5, 6, 7, 8};
	CHECK(cArrayFacet->Size(&raw) == 4);
	CHECK(*static_cast<const int*>(cArrayFacet->ElementRef(&raw, 3).Data) == 8);
}

TEST_CASE("a span reflects as a non-resizable sequence over its viewed elements")
{
	int storage[3] = {4, 5, 6};

	// Dynamic and static extent both match the one span specialization and expose the same shape.
	const PgE::SequenceFacet* dynamicFacet = PgE::TypeOf<std::span<int>>().GetFacet<PgE::SequenceFacet>();
	const PgE::SequenceFacet* staticFacet = PgE::TypeOf<std::span<int, 3>>().GetFacet<PgE::SequenceFacet>();
	REQUIRE(dynamicFacet != nullptr);
	REQUIRE(staticFacet != nullptr);

	// A view is not resizable: element access only, like std::array.
	CHECK_FALSE(dynamicFacet->CanClear());
	CHECK_FALSE(dynamicFacet->CanReserve());
	CHECK_FALSE(dynamicFacet->CanAppend());
	CHECK(dynamicFacet->CanMutateElements());
	CHECK(&dynamicFacet->ElementType() == &PgE::TypeOf<int>());

	std::span<int> view{storage};
	CHECK(dynamicFacet->Size(&view) == 3);

	// The element ref writes through the view into the underlying storage.
	const PgE::TypedRef second = dynamicFacet->ElementRef(&view, 1);
	*static_cast<int*>(second.Data) = 50;
	CHECK(storage[1] == 50);

	CHECK(PgE::ToString(std::span<int>{storage}) == "[4, 50, 6]");
}

TEST_CASE("a const-element span reflects as a read-only sequence")
{
	const int storage[3] = {7, 8, 9};
	const PgE::SequenceFacet* facet = PgE::TypeOf<std::span<const int>>().GetFacet<PgE::SequenceFacet>();
	REQUIRE(facet != nullptr);

	// Read-only: no mutation of any kind, but reads and the element type still resolve.
	CHECK_FALSE(facet->CanMutateElements());
	CHECK_FALSE(facet->CanClear());
	CHECK_FALSE(facet->CanAppend());
	CHECK(&facet->ElementType() == &PgE::TypeOf<int>());

	const std::span<const int> view{storage};
	CHECK(facet->Size(&view) == 3);
	const PgE::TypedRef first = facet->ElementRef(&view, 0);
	CHECK(first.IsConst);
	CHECK(*static_cast<const int*>(first.Data) == 7);

	CHECK(PgE::ToString(std::span<const int>{storage}) == "[7, 8, 9]");
}

TEST_CASE("nested sequences resolve their element facet lazily")
{
	std::vector<std::vector<int>> grid{{1, 2}, {3}};
	const PgE::SequenceFacet* outer = PgE::TypeOf<std::vector<std::vector<int>>>().GetFacet<PgE::SequenceFacet>();
	REQUIRE(outer != nullptr);

	// The element type is itself a sequence, reached through the lazy TypeReference.
	CHECK(&outer->ElementType() == &PgE::TypeOf<std::vector<int>>());
	REQUIRE(outer->ElementType().GetFacet<PgE::SequenceFacet>() != nullptr);
}

TEST_CASE("containers stringify element-wise through the sequence facet")
{
	CHECK(PgE::ToString(std::vector<int>{1, 2, 3}) == "[1, 2, 3]");
	CHECK(PgE::ToString(std::array<int, 2>{9, 8}) == "[9, 8]");
	CHECK(PgE::ToString(std::vector<std::vector<int>>{{1, 2}, {3}}) == "[[1, 2], [3]]");

	// A vector of strings composes the sequence and string facets: quoted elements in a bracketed list.
	CHECK(PgE::ToString(std::vector<std::string>{"a", "b"}) == R"(["a", "b"])");
}

TEST_CASE("a struct field that is a container stringifies through its facet")
{
	const Inventory inventory{.Slots = {1, 2, 3}, .Owner = "Ada"};

	// The field walk recurses into each field's facet: the vector renders as a list, the string quoted.
	CHECK(PgE::ToString(inventory) == R"({Slots: [1, 2, 3], Owner: "Ada"})");
}

TEST_CASE("a user-defined facet extends the system with no core change")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Labeled>();

	// The externally-defined facet is assembled into the table and keyed by its own type, reachable through
	// the same generic GetFacet the built-in facets use.
	const LabelFacet* label = type.GetFacet<LabelFacet>();
	REQUIRE(label != nullptr);
	CHECK(label->Text == "the-label");

	// It does not declare Supersedes, so the structural field view is kept alongside it.
	REQUIRE(type.FindFieldByIdentifier("Value") != nullptr);
	const Labeled instance{};
	CHECK(*type.GetFieldAs<int>(&instance, "Value") == 1);
}
