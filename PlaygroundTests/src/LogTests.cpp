#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Log;

// Characterizes the parser that turns source_location::function_name() into the qualified name shown in
// every log line. Inputs are literal GCC-16 signature strings (compiler-specific), so these also act as
// a tripwire for toolchain churn that would silently corrupt logged names.
TEST_CASE("ExtractQualifiedName reduces a signature to its qualified name")
{
	using PgE::detail::ExtractQualifiedName;

	// Return type stripped, parameter list dropped.
	CHECK(ExtractQualifiedName("void PgE::Foo()") == "PgE::Foo");
	CHECK(ExtractQualifiedName("int PgE::Bar::Baz()") == "PgE::Bar::Baz");

	// A constructor has no return type (no depth-0 space before the name).
	CHECK(ExtractQualifiedName("PgE::Bar::Bar()") == "PgE::Bar::Bar");

	// Template arguments must not be mistaken for the return-type space or params '('.
	CHECK(ExtractQualifiedName("void PgE::Container<int>::Add(int)") == "PgE::Container<int>::Add");
	CHECK(ExtractQualifiedName("std::vector<int> PgE::Foo::Get()") == "PgE::Foo::Get");

	// A leading '*'/'&' from a pointer/reference return type is trimmed off the name.
	CHECK(ExtractQualifiedName("int *PgE::Foo::Get()") == "PgE::Foo::Get");

	// GCC decorates module-local entities with "@Module.Name"; it is skipped.
	CHECK(ExtractQualifiedName("void PgE::Foo@PlaygroundEngine.Log::Bar()") == "PgE::Foo::Bar");
}
