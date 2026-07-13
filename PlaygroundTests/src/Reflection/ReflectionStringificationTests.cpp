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

TEST_CASE("non-formattable leaf falls back to its type name")
{
	// Opaque has no fields, no std::format support, and is not an enum, so it hits the type-name fallback.
	// The fixtures live in an importable module, so the reflected name carries GCC's module-ownership suffix.
	CHECK(PgE::ToString(Opaque{}) == "<ReflectionTestTypes::Opaque@PlaygroundTests.ReflectionTestTypes>");
}
