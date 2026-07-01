#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Reflection;

// Exercises the full reflection pipeline (TypeOfMeta -> display_string_of) and the
// import std + engine-module + doctest combination in one TU. A fundamental type is
// used because its display name is stable across implementations, unlike a class's
// (which may be namespace-qualified).
TEST_CASE("reflected type name is correct")
{
    CHECK(PgE::TypeOf<int>().GetDisplayName() == "int");
}

// A named namespace (not anonymous) so reflected member display strings are
// deterministic — field names currently come out fully qualified (see below).
namespace ReflectionTestTypes
{
	// ReSharper disable CppDeclaratorNeverUsed
	// ReSharper disable CppParameterMayBeConst
    // Mirrors the shape the game used to probe at startup: a string leaf and an int.
    struct Person
    {
	    std::string Name;
	    int Age;
    };

    // Fundamental-typed members only, so reflected signatures have stable display
    // strings (std::string's display name is implementation-defined).
    struct Widget
    {
        int Width;
        int Height;

        int Area() const { return Width * Height; }
        void Resize(int w, int h) { Width = w; Height = h; }
    };
	// ReSharper restore CppParameterMayBeConst
	// ReSharper restore CppDeclaratorNeverUsed
}

using ReflectionTestTypes::Person;
using ReflectionTestTypes::Widget;

// Leaf traits: ints stringify via std::format, strings are quoted.
TEST_CASE("leaf values stringify through TypeInfoTraits")
{
    CHECK(PgE::ToString(42) == "42");
    CHECK(PgE::ToString(std::string{"hello"}) == R"("hello")");
}

// The field walk in TypeInfo::ObjectToString recurses into each member, applies the
// leaf trait per field, and wraps the aggregate in braces. NOTE: field names are
// currently the fully-qualified display_string_of (Builder.cppm MakeField), unlike
// function names/params which use the short identifier_of — pin the current behavior.
TEST_CASE("object stringification walks fields")
{
    const Person person{"John", 25};

    CHECK(PgE::ToString(person)
          == R"({ReflectionTestTypes::Person::Name: "John", ReflectionTestTypes::Person::Age: 25})");
}

// Function reflection: return type, name, and parameter (type + name) for each
// non-special member function, in declaration order.
TEST_CASE("function reflection renders signatures")
{
    CHECK(PgE::TypeOf<Widget>().FunctionsToString() == "int Area()\nvoid Resize(int w, int h)");
}
