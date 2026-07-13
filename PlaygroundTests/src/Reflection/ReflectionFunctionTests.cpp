#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Reflection;
import PlaygroundTests.ReflectionTestTypes;

using namespace ReflectionTestTypes;

TEST_CASE("reflected member function invokes and returns")
{
	Widget widget{3, 4};

	const std::vector<const PgE::FunctionInfo*> areas = PgE::TypeOf<Widget>().FindFunctionsByIdentifier("Area");
	REQUIRE(areas.size() == 1);

	const auto result = areas.front()->InvokeAs<int>(&widget);

	REQUIRE(result.has_value());
	CHECK(*result == 12);
}

TEST_CASE("reflected member function mutates its instance")
{
	Widget widget{1, 1};

	const PgE::FunctionInfo* resize = PgE::TypeOf<Widget>().FindFunctionsByIdentifier("Resize").front();
	const auto result = resize->InvokeAs(&widget, 2, 5);

	REQUIRE(result.has_value());
	CHECK(widget.Width == 2);
	CHECK(widget.Height == 5);
}

TEST_CASE("invoke reports arity and type mismatches")
{
	Widget widget{};
	const PgE::FunctionInfo* resize = PgE::TypeOf<Widget>().FindFunctionsByIdentifier("Resize").front();

	int width = 2;
	const PgE::TypedRef tooFew[] = {{.Type = &PgE::TypeOf<int>(), .Data = &width, .IsConst = false}};
	const auto arity = resize->Invoke(&widget, tooFew);
	REQUIRE_FALSE(arity.has_value());
	CHECK(arity.error().Reason == PgE::InvokeError::ArityMismatch);

	int height = 5;
	std::string wrong = "x";
	const PgE::TypedRef mistyped[] = {{.Type = &PgE::TypeOf<std::string>(), .Data = &wrong, .IsConst = false},
									  {.Type = &PgE::TypeOf<int>(), .Data = &height, .IsConst = false}};
	const auto type = resize->Invoke(&widget, mistyped);
	REQUIRE_FALSE(type.has_value());
	CHECK(type.error().Reason == PgE::InvokeError::TypeMismatch);
	CHECK(type.error().ArgumentIndex == 0);
}

TEST_CASE("const object reaches only const-callable functions")
{
	Counter counter{7};
	const Counter& readOnly = counter;
	const PgE::TypeInfo& type = PgE::TypeOf<Counter>();

	const auto value = type.FindFunctionsByIdentifier("Get").front()->InvokeAs<int>(&readOnly);
	REQUIRE(value.has_value());
	CHECK(*value == 7);

	const auto mutated = type.FindFunctionsByIdentifier("Add").front()->InvokeAs(&readOnly, 1);
	REQUIRE_FALSE(mutated.has_value());
	CHECK(mutated.error().Reason == PgE::InvokeError::ConstViolation);
	CHECK(counter.Value == 7);
}

TEST_CASE("reflected function returning a reference yields a live reference")
{
	Accessor accessor{};

	const auto mutableRef = PgE::TypeOf<Accessor>().FindFunctionsByIdentifier("Mutable").front()->InvokeAs<int&>(&accessor);
	REQUIRE(mutableRef.has_value());
	int& reference = mutableRef.value();
	reference = 42;
	CHECK(accessor.Value == 42);

	const auto view = PgE::TypeOf<Accessor>().FindFunctionsByIdentifier("Readonly").front()->InvokeAs<const int&>(&accessor);
	REQUIRE(view.has_value());
	CHECK(view.value() == 42);
}

TEST_CASE("value-return sugar on a void function reports ReturnTypeMismatch")
{
	Widget widget{};
	const PgE::FunctionInfo* resize = PgE::TypeOf<Widget>().FindFunctionsByIdentifier("Resize").front();

	const auto result = resize->InvokeAs<int>(&widget, 2, 5);
	REQUIRE_FALSE(result.has_value());
	CHECK(result.error().Reason == PgE::InvokeError::ReturnTypeMismatch);
	CHECK(widget.Width == 0);
}

TEST_CASE("const argument to a move-only parameter is rejected")
{
	Sink sink{};
	const PgE::FunctionInfo* consume = PgE::TypeOf<Sink>().FindFunctionsByIdentifier("Consume").front();

	MoveOnly readOnlyArg;
	const PgE::TypedRef constArg[] = {{&PgE::TypeOf<MoveOnly>(), &readOnlyArg, true}};
	const auto rejected = consume->Invoke(&sink, constArg);
	REQUIRE_FALSE(rejected.has_value());
	CHECK(rejected.error().Reason == PgE::InvokeError::ConstViolation);

	MoveOnly ownedArg;
	ownedArg.Tag = 9;
	const PgE::TypedRef mutableArg[] = {{&PgE::TypeOf<MoveOnly>(), &ownedArg, false}};
	const auto accepted = consume->Invoke(&sink, mutableArg);
	REQUIRE(accepted.has_value());
	CHECK(sink.Value == 9);
}

TEST_CASE("reflection invokes private member functions")
{
	const PgE::TypeInfo& type = PgE::TypeOf<WithPrivate>();
	WithPrivate obj{21};

	const PgE::FunctionInfo* doubled = type.FindFunctionsByIdentifier("Doubled").front();
	const auto read = doubled->InvokeAs<int>(&obj);
	REQUIRE(read.has_value());
	CHECK(*read == 42);

	const PgE::FunctionInfo* setValue = type.FindFunctionsByIdentifier("SetValue").front();
	REQUIRE(setValue->InvokeAs(&obj, 5).has_value());
	CHECK(obj.Value == 5);
}

TEST_CASE("a function the thunk cannot call reflects as metadata but is not invocable")
{
	const PgE::TypeInfo& type = PgE::TypeOf<RefQualified>();
	RefQualified obj{7};

	// The rvalue-ref-qualified overload is reflected, but invoking it reports NotInvocable rather than
	// failing to compile or crashing on a null thunk.
	const PgE::FunctionInfo* onRvalue = type.FindFunctionsByIdentifier("OnRvalue").front();
	const auto rejected = onRvalue->InvokeAs<int>(&obj);
	REQUIRE_FALSE(rejected.has_value());
	CHECK(rejected.error().Reason == PgE::InvokeError::NotInvocable);

	// A normal overload on the same type still invokes.
	CHECK(type.FindFunctionsByIdentifier("OnAny").front()->InvokeAs<int>(&obj).value() == 7);
}

TEST_CASE("invoking through a base type calls the overwritten function")
{
	Child child{};
	ChildNoOverride childNoOverride{};
	const PgE::TypeInfo& baseType = PgE::TypeOf<Base>();
	const PgE::TypeInfo& childType = PgE::TypeOf<Child>();
	const PgE::TypeInfo& childNoOverrideType = PgE::TypeOf<ChildNoOverride>();

	auto baseFunctions = baseType.FindFunctionsByIdentifier("GetV");
	auto childFunctions = childType.FindFunctionsByIdentifier("GetV");
	auto childNoOverrideFunctions = childNoOverrideType.FindFunctionsByIdentifier("GetV");

	REQUIRE(baseFunctions.size() == 1);
	REQUIRE(childFunctions.size() == 1);
	REQUIRE(childNoOverrideFunctions.size() == 0);
	REQUIRE(baseFunctions.front()->InvokeAs<int>(&child).has_value());
	REQUIRE(baseFunctions.front()->InvokeAs<int>(&childNoOverride).has_value());
	REQUIRE(childFunctions.front()->InvokeAs<int>(&child).has_value());
	CHECK(baseFunctions.front()->InvokeAs<int>(&child).value() == 2);
	CHECK(baseFunctions.front()->InvokeAs<int>(&childNoOverride).value() == 1);
	CHECK(childFunctions.front()->InvokeAs<int>(&child).value() == 2);
}

TEST_CASE("invoke moves a by-value argument only for rvalues")
{
	Consumer consumer{};
	const PgE::FunctionInfo* store = PgE::TypeOf<Consumer>().FindFunctionsByIdentifier("Store").front();

	Tracked byCopy;
	byCopy.Value = 3;
	REQUIRE(store->InvokeAs(&consumer, byCopy).has_value());
	CHECK(consumer.Stored == 3);
	CHECK_FALSE(byCopy.Moved);

	Tracked byMove;
	byMove.Value = 8;
	REQUIRE(store->InvokeAs(&consumer, std::move(byMove)).has_value());
	CHECK(consumer.Stored == 8);
	CHECK(byMove.Moved);
}

TEST_CASE("InvokeAs moves a move-only argument into a parameter")
{
	Sink sink{};
	MoveOnly item;
	item.Tag = 4;

	const PgE::FunctionInfo* consume = PgE::TypeOf<Sink>().FindFunctionsByIdentifier("Consume").front();
	REQUIRE(consume->InvokeAs(&sink, std::move(item)).has_value());
	CHECK(sink.Value == 4);
}

TEST_CASE("invoke copies a by-value argument with a deleted move constructor")
{
	CopyConsumer consumer{};
	CopyOnlyParam argument;
	argument.Value = 6;

	const PgE::FunctionInfo* take = PgE::TypeOf<CopyConsumer>().FindFunctionsByIdentifier("Take").front();
	REQUIRE(take->InvokeAs(&consumer, argument).has_value());
	CHECK(consumer.Stored == 6);
}
