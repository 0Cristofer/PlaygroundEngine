#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Reflection;
import PlaygroundTests.ReflectionTestTypes;

using namespace ReflectionTestTypes;

TEST_CASE("a type whose signature names itself reflects and resolves back to itself")
{
	const PgE::TypeInfo& node = PgE::TypeOf<Node>();

	// A factory returning the type by value: the return type resolves to the very TypeInfo under
	// construction, the case that used to fail the consteval build.
	const PgE::FunctionInfo* clone = node.FindFunctionsByIdentifier("Clone").front();
	CHECK(&clone->GetReturnType() == &node);

	// A self-typed reference parameter and a self-returning-by-reference method resolve the same way,
	// since the builder erases cvref before keying the reference.
	const PgE::FunctionInfo* copyFrom = node.FindFunctionsByIdentifier("CopyFrom").front();
	CHECK(&copyFrom->GetParams().front().GetTypeInfo() == &node);
	CHECK(&node.FindFunctionsByIdentifier("Self").front()->GetReturnType() == &node);

	// The reflected factory is still callable end to end.
	Node original{7};
	const auto cloned = clone->InvokeAs<Node>(&original);
	REQUIRE(cloned.has_value());
	CHECK(cloned->Value == 7);
}

TEST_CASE("mutually referential types reflect without a construction cycle")
{
	const PgE::TypeInfo& ping = PgE::TypeOf<Ping>();
	const PgE::TypeInfo& pong = PgE::TypeOf<Pong>();

	CHECK(&ping.FindFunctionsByIdentifier("Bounce").front()->GetReturnType() == &pong);
	CHECK(&pong.FindFunctionsByIdentifier("Bounce").front()->GetReturnType() == &ping);
}
