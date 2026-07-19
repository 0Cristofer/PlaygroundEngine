#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Reflection;
import PlaygroundTests.ReflectionTestTypes;

using namespace ReflectionTestTypes;

TEST_CASE("overloaded operators reflect into their own list keyed by kind")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Coord>();

	REQUIRE(type.FindOperators(PgE::OperatorKind::Plus).size() == 1);
	REQUIRE(type.FindOperators(PgE::OperatorKind::Equal).size() == 1);
	REQUIRE(type.FindOperators(PgE::OperatorKind::Subscript).size() == 1);
	REQUIRE(type.FindOperators(PgE::OperatorKind::Assign).size() == 1);

	// operator- is private but still reflected and keyed, like a private member function.
	REQUIRE(type.FindOperators(PgE::OperatorKind::Minus).size() == 1);
	CHECK(type.FindOperators(PgE::OperatorKind::Minus).front()->GetAccess() == PgE::AccessKind::Private);

	// Copy and move assignment are special members, so they are not in the operator list even though they
	// are operator functions.
	CHECK(type.FindOperators(PgE::OperatorKind::Assign).front()->GetParams().size() == 1);

	// Coord declares a hidden friend operator*, which members_of does not surface: it is not a class member,
	// only findable by argument-dependent lookup. Free operators are reached the way free functions are.
	CHECK(type.FindOperators(PgE::OperatorKind::Multiply).empty());
	CHECK(Coord{10} * Coord{3} == Coord{30});
}

TEST_CASE("reflected operators invoke through the shared function machinery")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Coord>();
	Coord left{10};
	Coord right{3};

	const PgE::OperatorInfo* plus = type.FindOperators(PgE::OperatorKind::Plus).front();
	CHECK(plus->InvokeAs<Coord>(&left, right)->Value == 13);

	const PgE::OperatorInfo* equal = type.FindOperators(PgE::OperatorKind::Equal).front();
	CHECK(equal->InvokeAs<bool>(&left, right).value() == false);
	CHECK(equal->InvokeAs<bool>(&left, Coord{10}).value() == true);

	const PgE::OperatorInfo* subscript = type.FindOperators(PgE::OperatorKind::Subscript).front();
	CHECK(subscript->InvokeAs<int>(&left, 5).value() == 15);

	// The private operator invokes through the pointer route, like a private member function.
	const PgE::OperatorInfo* minus = type.FindOperators(PgE::OperatorKind::Minus).front();
	CHECK(minus->InvokeAs<Coord>(&left, right)->Value == 7);

	// The converting assignment mutates the object and returns a reference to it.
	const PgE::OperatorInfo* assign = type.FindOperators(PgE::OperatorKind::Assign).front();
	REQUIRE(assign->InvokeAs<Coord&>(&left, 99).has_value());
	CHECK(left.Value == 99);
}

TEST_CASE("user-defined conversions reflect with their target type and explicit flag")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Coord>();

	const PgE::ConversionInfo* toInt = nullptr;
	const PgE::ConversionInfo* toBool = nullptr;
	for (const PgE::ConversionInfo& conversion : type.GetConversions())
	{
		if (&conversion.GetTargetType() == &PgE::TypeOf<int>())
		{
			toInt = &conversion;
		}
		else if (&conversion.GetTargetType() == &PgE::TypeOf<bool>())
		{
			toBool = &conversion;
		}
	}

	REQUIRE(toInt != nullptr);
	REQUIRE(toBool != nullptr);

	CHECK(toInt->IsExplicit());
	CHECK_FALSE(toBool->IsExplicit());

	Coord coord{42};
	CHECK(toInt->InvokeAs<int>(&coord).value() == 42);
	CHECK(toBool->InvokeAs<bool>(&coord).value() == true);
}

TEST_CASE("operators and conversions do not leak into the ordinary function list")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Coord>();

	// Coord declares only operators and conversions, so its ordinary-function list is empty: a conversion no
	// longer appears there nameless, and operators are keyed in their own list.
	CHECK(type.GetFunctions().empty());

	CHECK_FALSE(type.GetOperators().empty());
	CHECK_FALSE(type.GetConversions().empty());
}
