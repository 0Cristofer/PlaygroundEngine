#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Reflection;
import PlaygroundTests.ReflectionTestTypes;

using namespace ReflectionTestTypes;

TEST_CASE("leaf values stringify through TypeInfoTraits")
{
	CHECK(PgE::ToString(42) == "42");
	CHECK(PgE::ToString(std::string{"hello"}) == R"("hello")");
}

TEST_CASE("object stringification walks fields")
{
	const Person person{"John", 25};

	CHECK(PgE::ToString(person) == R"({Name: "John", Age: 25})");
}

TEST_CASE("object stringification reads bitfields and references correctly")
{
	constexpr Packet packet{};
	CHECK(PgE::ToString(packet) == "{Version: 3, Flags: 10, Payload: 100}");

	const Referencing referencing{};
	CHECK(PgE::ToString(referencing) == "{Target: 100, Alias: 100, ConstAlias: 100}");
}

TEST_CASE("object stringification walks bases for inherited fields")
{
	// RootB and RootC sit at nonzero offsets, so reading their fields proves the walk reaches each base
	// subobject rather than reinterpreting the derived address. Access does not filter: a protected and a
	// private base render like a public one, matching the field walk.
	constexpr MultiDerived multiDerived{};
	CHECK(PgE::ToString(multiDerived) == "{A: 1, B: 2, C: 3, Own: 0}");

	// Inherited fields are flattened in declaration order, bases first, and the flattening is recursive.
	constexpr Grandchild grandchild{};
	CHECK(PgE::ToString(grandchild) == "{A: 1, B: 2, C: 3, Own: 0, G: 0}");
}

TEST_CASE("a type whose fields all come from a base renders them, not a placeholder")
{
	// The builder still classifies this as a leaf and gives it a stringify thunk (it has no fields of its
	// own). Preferring the walk whenever there is something to walk is the renderer's decision, so the thunk
	// stays unused here rather than rendering the type-name placeholder.
	constexpr InheritedOnly inheritedOnly{};

	CHECK(PgE::TypeOf<InheritedOnly>().GetFields().empty());
	CHECK(PgE::TypeOf<InheritedOnly>().CanStringify());
	CHECK(PgE::ToString(inheritedOnly) == "{A: 1}");
}

TEST_CASE("an empty base contributes no entry")
{
	// A tag or policy base holds no state, so it adds nothing rather than the placeholder its leaf thunk
	// would produce.
	constexpr TaggedValue tagged{};

	CHECK(PgE::TypeOf<TaggedValue>().GetBases().size() == 1);
	CHECK(PgE::ToString(tagged) == "{Value: 5}");
}

TEST_CASE("a base that renders itself stays one named entry")
{
	DerivedFromString derived;
	derived.assign("hello");

	// The label is the base's display name, because a specialization has no identifier of its own. That text
	// is implementation-defined (libstdc++'s dual-ABI inline namespace shows up in it), so the expectation is
	// derived from the API rather than spelled out: what this pins is the fallback, not the spelling.
	CHECK(PgE::TypeOf<std::string>().GetIdentifier().empty());

	const std::string expected = std::format(R"({{{}: "hello", Tag: 7}})", PgE::TypeOf<std::string>().GetDisplayName());
	CHECK(PgE::ToString(derived) == expected);
}

TEST_CASE("non-formattable leaf falls back to its type name")
{
	// Opaque has no fields, no std::format support, and is not an enum, so it hits the type-name fallback.
	// The fixtures live in an importable module, so the reflected name carries GCC's module-ownership suffix.
	CHECK(PgE::ToString(Opaque{}) == "<ReflectionTestTypes::Opaque@PlaygroundTests.ReflectionTestTypes>");
}
