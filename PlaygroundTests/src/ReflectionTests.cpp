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
