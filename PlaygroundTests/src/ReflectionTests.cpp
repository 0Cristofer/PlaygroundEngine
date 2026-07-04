#include <doctest/doctest.h>
#include <meta>

import std;
import PlaygroundEngine.Reflection;

// A named namespace (not anonymous) so reflected member display strings are deterministic across runs.
namespace ReflectionTestTypes
{
	// ReSharper disable CppDeclaratorNeverUsed
	// ReSharper disable CppParameterMayBeConst
	// ReSharper disable CppPassValueParameterByConstReference
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

		void Resize(int w, int h)
		{
			Width = w;
			Height = h;
		}
	};

	struct Counter
	{
		int Value = 0;

		int Get() const { return Value; }

		void Add(int amount) { Value += amount; }
	};

	struct Accessor
	{
		int Value = 5;

		int& Mutable() { return Value; }

		const int& Readonly() const { return Value; }
	};

	struct MoveOnly
	{
		int Tag = 0;

		MoveOnly() = default;
		MoveOnly(const MoveOnly&) = delete;
		MoveOnly(MoveOnly&&) = default;
	};

	struct Sink
	{
		int Value = 0;

		void Consume(MoveOnly item) { Value = item.Tag; }
	};

	// Bitfields (no addressable byte) plus a plain field, to prove field access does not depend on
	// taking a member's address.
	struct Packet
	{
		unsigned Version : 4 = 3;
		unsigned Flags : 4 = 10;
		int Payload = 100;
	};

	// Private members, to prove reflected access ignores access control (the point of the feature).
	class Secret
	{
	public:
		int Reveal() const { return _hidden; }

	private:
		int _hidden = 7;
	};

	// A const member has no setter (NotWritable) but is still readable.
	struct Fixed
	{
		const int Constant = 5;
		int Variable = 1;
	};

	// Reference members: tagged by the referent type, read through the reference, never rebindable.
	struct Referencing
	{
		int Target = 100;
		int& Alias;
		const int& ConstAlias;

		Referencing() : Alias(Target), ConstAlias(Target)
		{
		}
	};

	// Move-only but move-assignable (like unique_ptr / Poly): settable through a move, not a copy.
	struct MoveOnlyValue
	{
		int Tag = 0;
		MoveOnlyValue() = default;
		MoveOnlyValue(const MoveOnlyValue&) = delete;
		MoveOnlyValue(MoveOnlyValue&&) = default;
		MoveOnlyValue& operator=(MoveOnlyValue&&) = default;
	};

	struct MoveOnlyHolder
	{
		MoveOnlyValue Item;
	};

	// Neither copyable nor movable (like std::mutex): reachable only through the borrow.
	struct Immovable
	{
		int Tag = 0;
		Immovable() = default;
		Immovable(const Immovable&) = delete;
		Immovable(Immovable&&) = delete;
		Immovable& operator=(const Immovable&) = delete;
		Immovable& operator=(Immovable&&) = delete;
	};

	struct ImmovableHolder
	{
		Immovable Item;
	};

	// A nested reflected struct plus a pointer member.
	struct Inner
	{
		int A = 1;
	};

	struct Outer
	{
		Inner Child;
		int* Pointer = nullptr;
		int Plain = 7;
	};

	// Tracks whether an instance was moved-from, to observe "invoke"'s opt-in move.
	struct Tracked
	{
		int Value = 0;
		bool Moved = false;
		Tracked() = default;

		Tracked(const Tracked& other) : Value(other.Value)
		{
		}

		Tracked(Tracked&& other) noexcept : Value(other.Value) { other.Moved = true; }

		Tracked& operator=(const Tracked&) = default;
		Tracked& operator=(Tracked&&) = default;
	};

	struct Consumer
	{
		int Stored = 0;

		void Store(Tracked item) { Stored = item.Value; }
	};

	// Copyable but with a deleted move constructor: the invoke binder must still copy it by value.
	struct CopyOnlyParam
	{
		int Value = 0;
		CopyOnlyParam() = default;
		CopyOnlyParam(const CopyOnlyParam&) = default;
		CopyOnlyParam(CopyOnlyParam&&) = delete;
		CopyOnlyParam& operator=(const CopyOnlyParam&) = default;
	};

	struct CopyConsumer
	{
		int Stored = 0;

		void Take(CopyOnlyParam item) { Stored = item.Value; }
	};

	// A leaf with no std::format support and no trait: reflectable, stringified by type-name fallback.
	enum class Shade { Red, Green };

	struct Palette
	{
		Shade Primary = Shade::Green;
		int Count = 2;
	};

	struct Base
	{
		virtual ~Base() = default;
		int V = 1;

		virtual int GetV()
		{
			return V;
		}
	};

	struct Child : Base
	{
		int GetV() override
		{
			return V + 1;
		}
	};

	struct ChildNoOverride : Base
	{
	};

	// Value-carrying annotations plus an empty tag. Doc stores its string through
	// define_static_string so the value is a self-contained constant (see the reflection
	// design notes on structural annotation values).
	struct Range
	{
		double Min;
		double Max;
	};

	struct Doc
	{
		const char* Text;

		consteval Doc(const char* text) : Text(std::define_static_string(std::string_view{text}))
		{
		}
	};

	struct Serializable
	{
	};

	// One type carrying annotations on every declaration kind: the type itself, a field,
	// a member function, and a parameter. Exercises the shared Annotated surface uniformly.
	struct [[=Serializable{}]] Gadget
	{
		[[=Range{0.0, 100.0}]] [[=Doc{"hit points"}]]
		int Health = 100;

		// The same annotation type repeated: legal, kept in declaration order, not deduplicated.
		[[=Doc{"primary"}]] [[=Doc{"secondary"}]]
		int Nickname = 0;

		int Plain = 0;

		[[=Doc{"apply damage, return remaining health"}]]
		int Hurt([[=Range{0.0, 1000.0}]] int amount)
		{
			Health -= amount;
			return Health;
		}
	};

	// ReSharper restore CppParameterMayBeConst
	// ReSharper restore CppDeclaratorNeverUsed
	// ReSharper restore CppPassValueParameterByConstReference
}

using ReflectionTestTypes::Person;
using ReflectionTestTypes::Widget;
using ReflectionTestTypes::Counter;
using ReflectionTestTypes::Accessor;
using ReflectionTestTypes::MoveOnly;
using ReflectionTestTypes::Sink;
using ReflectionTestTypes::Packet;
using ReflectionTestTypes::Secret;
using ReflectionTestTypes::Fixed;
using ReflectionTestTypes::Referencing;
using ReflectionTestTypes::MoveOnlyValue;
using ReflectionTestTypes::MoveOnlyHolder;
using ReflectionTestTypes::Immovable;
using ReflectionTestTypes::ImmovableHolder;
using ReflectionTestTypes::Inner;
using ReflectionTestTypes::Outer;
using ReflectionTestTypes::Tracked;
using ReflectionTestTypes::Consumer;
using ReflectionTestTypes::CopyOnlyParam;
using ReflectionTestTypes::CopyConsumer;
using ReflectionTestTypes::Shade;
using ReflectionTestTypes::Palette;
using ReflectionTestTypes::Base;
using ReflectionTestTypes::Child;
using ReflectionTestTypes::ChildNoOverride;
using ReflectionTestTypes::Range;
using ReflectionTestTypes::Doc;
using ReflectionTestTypes::Serializable;
using ReflectionTestTypes::Gadget;

TEST_CASE("reflected type name is correct")
{
	CHECK(PgE::TypeOf<int>().GetDisplayName() == "int");
}

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

TEST_CASE("function reflection renders signatures")
{
	CHECK(PgE::TypeOf<Widget>().FunctionsToString() == "int Area()\nvoid Resize(int w, int h)");
}

TEST_CASE("reflected member function invokes and returns")
{
	Widget widget{3, 4};

	const std::vector<const PgE::FuncInfo*> areas = PgE::TypeOf<Widget>().FindFunctionsByIdentifier("Area");
	REQUIRE(areas.size() == 1);

	const auto result = areas.front()->InvokeAs<int>(&widget);

	REQUIRE(result.has_value());
	CHECK(*result == 12);
}

TEST_CASE("reflected member function mutates its instance")
{
	Widget widget{1, 1};

	const PgE::FuncInfo* resize = PgE::TypeOf<Widget>().FindFunctionsByIdentifier("Resize").front();
	const auto result = resize->InvokeAs(&widget, 2, 5);

	REQUIRE(result.has_value());
	CHECK(widget.Width == 2);
	CHECK(widget.Height == 5);
}

TEST_CASE("invoke reports arity and type mismatches")
{
	Widget widget{};
	const PgE::FuncInfo* resize = PgE::TypeOf<Widget>().FindFunctionsByIdentifier("Resize").front();

	int width = 2;
	const PgE::TypedRef tooFew[] = {{.Type = &PgE::TypeOf<int>(), .Data = &width, .IsConst = false}};
	const auto arity = resize->Invoke(&widget, tooFew);
	REQUIRE_FALSE(arity.has_value());
	CHECK(arity.error().Reason == PgE::InvokeError::ArityMismatch);

	int height = 5;
	std::string wrong = "x";
	const PgE::TypedRef mistyped[] = {
		{.Type = &PgE::TypeOf<std::string>(), .Data = &wrong, .IsConst = false},
		{.Type = &PgE::TypeOf<int>(), .Data = &height, .IsConst = false}
	};
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

	const auto mutableRef = PgE::TypeOf<Accessor>().FindFunctionsByIdentifier("Mutable").front()->InvokeAs<int
		&>(&accessor);
	REQUIRE(mutableRef.has_value());
	int& reference = mutableRef.value();
	reference = 42;
	CHECK(accessor.Value == 42);

	const auto view = PgE::TypeOf<Accessor>().FindFunctionsByIdentifier("Readonly").front()->InvokeAs<const int
		&>(&accessor);
	REQUIRE(view.has_value());
	CHECK(view.value() == 42);
}

TEST_CASE("value-return sugar on a void function reports ReturnTypeMismatch")
{
	Widget widget{};
	const PgE::FuncInfo* resize = PgE::TypeOf<Widget>().FindFunctionsByIdentifier("Resize").front();

	const auto result = resize->InvokeAs<int>(&widget, 2, 5);
	REQUIRE_FALSE(result.has_value());
	CHECK(result.error().Reason == PgE::InvokeError::ReturnTypeMismatch);
	CHECK(widget.Width == 0);
}

TEST_CASE("const argument to a move-only parameter is rejected")
{
	Sink sink{};
	const PgE::FuncInfo* consume = PgE::TypeOf<Sink>().FindFunctionsByIdentifier("Consume").front();

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

TEST_CASE("typed field access reads and writes by name")
{
	Person person{"John", 25};
	const PgE::TypeInfo& type = PgE::TypeOf<Person>();

	const auto age = type.GetFieldAs<int>(&person, "Age");
	REQUIRE(age.has_value());
	CHECK(*age == 25);

	REQUIRE(type.SetFieldAs(&person, "Age", 30).has_value());
	CHECK(person.Age == 30);

	const auto name = type.GetFieldAs<std::string>(&person, "Name");
	REQUIRE(name.has_value());
	CHECK(*name == "John");
}

TEST_CASE("bitfields read and write like any field")
{
	Packet packet{};
	const PgE::TypeInfo& type = PgE::TypeOf<Packet>();

	CHECK(*type.GetFieldAs<unsigned>(&packet, "Version") == 3u);
	CHECK(*type.GetFieldAs<unsigned>(&packet, "Flags") == 10u);

	REQUIRE(type.SetFieldAs(&packet, "Version", 9u).has_value());
	CHECK(packet.Version == 9u);
	CHECK(packet.Flags == 10u);
	CHECK(packet.Payload == 100);
}

TEST_CASE("private fields are readable and writable through reflection")
{
	Secret secret{};
	const PgE::TypeInfo& type = PgE::TypeOf<Secret>();

	const auto hidden = type.GetFieldAs<int>(&secret, "_hidden");
	REQUIRE(hidden.has_value());
	CHECK(*hidden == 7);

	REQUIRE(type.SetFieldAs(&secret, "_hidden", 42).has_value());
	CHECK(secret.Reveal() == 42);
}

TEST_CASE("erased field access moves values through a typed slot")
{
	Person person{.Name = "Jane", .Age = 40};
	const PgE::TypeInfo& type = PgE::TypeOf<Person>();

	int readBack = 0;
	const auto got = type.GetFieldValue(&person, "Age", {
		                                    .Type = &PgE::TypeOf<int>(), .Data = &readBack, .IsConst = false
	                                    });
	REQUIRE(got.has_value());
	CHECK(readBack == 40);

	int newAge = 41;
	const auto set = type.SetFieldValue(&person, "Age", {
		                                    .Type = &PgE::TypeOf<int>(), .Data = &newAge, .IsConst = true
	                                    });
	REQUIRE(set.has_value());
	CHECK(person.Age == 41);
}

TEST_CASE("field access reports missing names and type mismatches")
{
	Person person{.Name = "Jane", .Age = 40};
	const PgE::TypeInfo& type = PgE::TypeOf<Person>();

	const auto missing = type.GetFieldAs<int>(&person, "Nope");
	REQUIRE_FALSE(missing.has_value());
	CHECK(missing.error().Reason == PgE::FieldError::FieldNotFound);

	const auto mistyped = type.GetFieldAs<std::string>(&person, "Age");
	REQUIRE_FALSE(mistyped.has_value());
	CHECK(mistyped.error().Reason == PgE::FieldError::TypeMismatch);
}

TEST_CASE("const member is readable but not writable")
{
	Fixed fixed{};
	const PgE::TypeInfo& type = PgE::TypeOf<Fixed>();

	CHECK(*type.GetFieldAs<int>(&fixed, "Constant") == 5);

	const auto rejected = type.SetFieldAs(&fixed, "Constant", 9);
	REQUIRE_FALSE(rejected.has_value());
	CHECK(rejected.error().Reason == PgE::FieldError::NotWritable);
	CHECK(fixed.Constant == 5);

	REQUIRE(type.SetFieldAs(&fixed, "Variable", 9).has_value());
	CHECK(fixed.Variable == 9);
}

TEST_CASE("field access through a resolved FieldInfo")
{
	Person person{.Name = "John", .Age = 25};

	const PgE::FieldInfo* age = PgE::TypeOf<Person>().FindFieldByIdentifier("Age");
	REQUIRE(age != nullptr);
	CHECK(age->GetIdentifier() == "Age");
	CHECK(&age->GetTypeInfo() == &PgE::TypeOf<int>());

	const auto typed = age->GetAs<int>(&person);
	REQUIRE(typed.has_value());
	CHECK(*typed == 25);

	REQUIRE(age->SetAs(&person, 30).has_value());
	CHECK(person.Age == 30);

	int readBack = 0;
	REQUIRE(age->GetValue(&person, {&PgE::TypeOf<int>(), &readBack, false}).has_value());
	CHECK(readBack == 30);

	int newAge = 31;
	REQUIRE(age->SetValue(&person, {&PgE::TypeOf<int>(), &newAge, true}).has_value());
	CHECK(person.Age == 31);
}

TEST_CASE("FindFieldByIdentifier misses return nullptr")
{
	CHECK(PgE::TypeOf<Person>().FindFieldByIdentifier("Nope") == nullptr);
}

TEST_CASE("reference members read through to the referent and are not writable")
{
	Referencing referencing{};
	const PgE::TypeInfo& type = PgE::TypeOf<Referencing>();

	const PgE::FieldInfo* alias = type.FindFieldByIdentifier("Alias");
	REQUIRE(alias != nullptr);
	CHECK(&alias->GetTypeInfo() == &PgE::TypeOf<int>());

	CHECK(*type.GetFieldAs<int>(&referencing, "Alias") == 100);

	// Reading dereferences the live reference: mutating the referent shows through on the next read.
	referencing.Target = 55;
	CHECK(*type.GetFieldAs<int>(&referencing, "Alias") == 55);
	CHECK(*type.GetFieldAs<int>(&referencing, "ConstAlias") == 55);

	const auto rebindMutable = type.SetFieldAs(&referencing, "Alias", 7);
	REQUIRE_FALSE(rebindMutable.has_value());
	CHECK(rebindMutable.error().Reason == PgE::FieldError::NotWritable);

	const auto rebindConst = type.SetFieldAs(&referencing, "ConstAlias", 7);
	REQUIRE_FALSE(rebindConst.has_value());
	CHECK(rebindConst.error().Reason == PgE::FieldError::NotWritable);

	CHECK(referencing.Target == 55);
}

TEST_CASE("borrowed field reference reads and writes in place")
{
	Person person{"John", 25};
	const PgE::TypeInfo& type = PgE::TypeOf<Person>();

	const auto ref = type.GetFieldRef(&person, "Age");
	REQUIRE(ref.has_value());
	CHECK(ref->Data == &person.Age);
	CHECK_FALSE(ref->IsConst);

	auto age = type.GetFieldRefAs<int>(&person, "Age");
	REQUIRE(age.has_value());
	age->get() = 99;
	CHECK(person.Age == 99);
}

TEST_CASE("bitfields have no borrow")
{
	Packet packet{};
	const auto ref = PgE::TypeOf<Packet>().GetFieldRef(&packet, "Version");
	REQUIRE_FALSE(ref.has_value());
	CHECK(ref.error().Reason == PgE::FieldError::NotAddressable);
}

TEST_CASE("borrow constness follows the object and the member")
{
	Fixed fixed{};
	const PgE::TypeInfo& type = PgE::TypeOf<Fixed>();

	const auto viaConstObject = type.GetFieldRef(&std::as_const(fixed), "Variable");
	REQUIRE(viaConstObject.has_value());
	CHECK(viaConstObject->IsConst);

	const auto mutableConstMember = type.GetFieldRefAs<int>(&fixed, "Constant");
	REQUIRE_FALSE(mutableConstMember.has_value());
	CHECK(mutableConstMember.error().Reason == PgE::FieldError::NotWritable);

	const Fixed& readOnly = fixed;
	const auto readConstMember = type.GetFieldRefAs<int>(&readOnly, "Constant");
	REQUIRE(readConstMember.has_value());
	CHECK(readConstMember->get() == 5);
}

TEST_CASE("borrowed reference member assigns through to the referent")
{
	Referencing referencing{};
	const PgE::TypeInfo& type = PgE::TypeOf<Referencing>();

	const auto aliasRef = type.GetFieldRef(&referencing, "Alias");
	REQUIRE(aliasRef.has_value());
	CHECK(aliasRef->Data == &referencing.Target);
	CHECK_FALSE(aliasRef->IsConst);

	auto alias = type.GetFieldRefAs<int>(&referencing, "Alias");
	REQUIRE(alias.has_value());
	alias->get() = 77;
	CHECK(referencing.Target == 77);

	const auto constAlias = type.GetFieldRef(&referencing, "ConstAlias");
	REQUIRE(constAlias.has_value());
	CHECK(constAlias->IsConst);
}

TEST_CASE("move-only field is settable by move, not by copy")
{
	MoveOnlyHolder holder{};
	const PgE::TypeInfo& type = PgE::TypeOf<MoveOnlyHolder>();

	const auto valueGet = type.GetFieldAs<MoveOnlyValue>(&holder, "Item");
	REQUIRE_FALSE(valueGet.has_value());
	CHECK(valueGet.error().Reason == PgE::FieldError::NotReadable);

	MoveOnlyValue source;
	source.Tag = 9;
	const auto copySet = type.SetFieldAs(&holder, "Item", source);
	REQUIRE_FALSE(copySet.has_value());
	CHECK(copySet.error().Reason == PgE::FieldError::NotWritable);

	const auto moveSet = type.MoveFieldAs(&holder, "Item", source);
	REQUIRE(moveSet.has_value());
	CHECK(holder.Item.Tag == 9);

	auto borrow = type.GetFieldRefAs<MoveOnlyValue>(&holder, "Item");
	REQUIRE(borrow.has_value());
	CHECK(borrow->get().Tag == 9);
}

TEST_CASE("non-copyable non-movable field is reachable only through the borrow")
{
	ImmovableHolder holder{};
	const PgE::TypeInfo& type = PgE::TypeOf<ImmovableHolder>();

	const auto get = type.GetFieldValue(&holder, "Item", {&PgE::TypeOf<Immovable>(), nullptr, false});
	REQUIRE_FALSE(get.has_value());
	CHECK(get.error().Reason == PgE::FieldError::NotReadable);

	Immovable other;
	const auto move = type.MoveFieldAs(&holder, "Item", other);
	REQUIRE_FALSE(move.has_value());
	CHECK(move.error().Reason == PgE::FieldError::NotWritable);

	auto borrow = type.GetFieldRefAs<Immovable>(&holder, "Item");
	REQUIRE(borrow.has_value());
	borrow->get().Tag = 5;
	CHECK(holder.Item.Tag == 5);
}

// A nested reflected struct: value get/set copies the whole sub-object, the borrow mutates it in
// place, and stringification recurses. A pointer member reads and writes as a value.
TEST_CASE("nested struct and pointer fields")
{
	Outer outer{};
	const PgE::TypeInfo& type = PgE::TypeOf<Outer>();

	CHECK(type.GetFieldAs<Inner>(&outer, "Child")->A == 1);
	type.GetFieldRefAs<Inner>(&outer, "Child")->get().A = 99;
	CHECK(outer.Child.A == 99);

	int target = 0;
	REQUIRE(type.SetFieldAs(&outer, "Pointer", &target).has_value());
	CHECK(outer.Pointer == &target);
	CHECK(*type.GetFieldAs<int*>(&outer, "Pointer") == &target);

	outer.Child.A = 1;
	outer.Pointer = nullptr;
	CHECK(PgE::ToString(outer).starts_with("{Child: {A: 1}, Pointer: "));
	CHECK(PgE::ToString(outer).ends_with(", Plain: 7}"));
}

TEST_CASE("invoke moves a by-value argument only for rvalues")
{
	Consumer consumer{};
	const PgE::FuncInfo* store = PgE::TypeOf<Consumer>().FindFunctionsByIdentifier("Store").front();

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

	const PgE::FuncInfo* consume = PgE::TypeOf<Sink>().FindFunctionsByIdentifier("Consume").front();
	REQUIRE(consume->InvokeAs(&sink, std::move(item)).has_value());
	CHECK(sink.Value == 4);
}

TEST_CASE("invoke copies a by-value argument with a deleted move constructor")
{
	CopyConsumer consumer{};
	CopyOnlyParam argument;
	argument.Value = 6;

	const PgE::FuncInfo* take = PgE::TypeOf<CopyConsumer>().FindFunctionsByIdentifier("Take").front();
	REQUIRE(take->InvokeAs(&consumer, argument).has_value());
	CHECK(consumer.Stored == 6);
}

TEST_CASE("non-formattable leaf falls back to its type name")
{
	Palette palette{};
	const PgE::TypeInfo& type = PgE::TypeOf<Palette>();

	CHECK(*type.GetFieldAs<Shade>(&palette, "Primary") == Shade::Green);
	REQUIRE(type.SetFieldAs(&palette, "Primary", Shade::Red).has_value());
	CHECK(palette.Primary == Shade::Red);

	CHECK(PgE::ToString(palette) == "{Primary: <ReflectionTestTypes::Shade>, Count: 2}");
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

TEST_CASE("field annotations expose their values")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Gadget>();

	const PgE::FieldInfo* health = type.FindFieldByIdentifier("Health");
	REQUIRE(health != nullptr);

	const auto ranges = health->GetAnnotations<Range>();
	REQUIRE(ranges.size() == 1);
	CHECK(ranges.front()->Min == 0.0);
	CHECK(ranges.front()->Max == 100.0);

	const auto docs = health->GetAnnotations<Doc>();
	REQUIRE(docs.size() == 1);
	CHECK(std::string_view{docs.front()->Text} == "hit points");

	CHECK(health->HasAnnotation<Range>());
	CHECK(health->GetAnnotations().size() == 2);

	// A cv/ref-qualified query normalizes to the same stored annotation.
	CHECK(health->GetAnnotations<const Range&>() == ranges);
	CHECK(health->HasAnnotation<const Range>());
}

TEST_CASE("repeated annotations of one type are all returned in declaration order")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Gadget>();

	const PgE::FieldInfo* nickname = type.FindFieldByIdentifier("Nickname");
	REQUIRE(nickname != nullptr);

	const auto docs = nickname->GetAnnotations<Doc>();
	REQUIRE(docs.size() == 2);
	CHECK(std::string_view{docs[0]->Text} == "primary");
	CHECK(std::string_view{docs[1]->Text} == "secondary");
}

TEST_CASE("absent annotations query as empty")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Gadget>();

	const PgE::FieldInfo* plain = type.FindFieldByIdentifier("Plain");
	REQUIRE(plain != nullptr);

	CHECK(plain->GetAnnotations<Range>().empty());
	CHECK_FALSE(plain->HasAnnotation<Doc>());
	CHECK(plain->GetAnnotations().empty());

	// An annotated field is still empty for a type it does not carry.
	const PgE::FieldInfo* health = type.FindFieldByIdentifier("Health");
	REQUIRE(health != nullptr);
	CHECK(health->GetAnnotations<Serializable>().empty());
}

TEST_CASE("annotations attach to types, functions, and parameters alike")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Gadget>();

	// Type-level annotation, through the same Annotated surface.
	CHECK(type.HasAnnotation<Serializable>());
	CHECK(type.GetAnnotations<Serializable>().size() == 1);

	const auto functions = type.FindFunctionsByIdentifier("Hurt");
	REQUIRE(functions.size() == 1);
	const PgE::FuncInfo* hurt = functions.front();

	const auto functionDocs = hurt->GetAnnotations<Doc>();
	REQUIRE(functionDocs.size() == 1);
	CHECK(std::string_view{functionDocs.front()->Text} == "apply damage, return remaining health");

	// Parameter-level annotation.
	const std::span<const PgE::ParamInfo> params = hurt->GetParams();
	REQUIRE(params.size() == 1);
	const auto paramRanges = params.front().GetAnnotations<Range>();
	REQUIRE(paramRanges.size() == 1);
	CHECK(paramRanges.front()->Max == 1000.0);
}

TEST_CASE("identifier and display name are exposed separately on every declaration kind")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Gadget>();
	CHECK(type.GetIdentifier() == "Gadget");
	CHECK(type.GetDisplayName() == "ReflectionTestTypes::Gadget");

	// Fundamental types and template specializations have no identifier, only a display
	// name: their names are keywords or template-ids, not identifiers. Aliases like
	// std::string dissolve during template deduction, so TypeOf sees the specialization.
	CHECK(PgE::TypeOf<int>().GetIdentifier().empty());
	CHECK(PgE::TypeOf<int>().GetDisplayName() == "int");
	CHECK(PgE::TypeOf<std::string>().GetIdentifier().empty());

	// Compound types have no identifier either.
	CHECK(PgE::TypeOf<int*>().GetIdentifier().empty());
	CHECK(PgE::TypeOf<int*>().GetDisplayName() == "int*");
	CHECK(PgE::TypeOf<int[4]>().GetIdentifier().empty());

	const PgE::FieldInfo* health = type.FindFieldByIdentifier("Health");
	REQUIRE(health != nullptr);
	CHECK(health->GetIdentifier() == "Health");
	CHECK(health->GetDisplayName() == "ReflectionTestTypes::Gadget::Health");

	const auto functions = type.FindFunctionsByIdentifier("Hurt");
	REQUIRE(functions.size() == 1);
	CHECK(functions.front()->GetIdentifier() == "Hurt");
	CHECK(functions.front()->GetDisplayName() == "int ReflectionTestTypes::Gadget::Hurt(int)");

	const std::span<const PgE::ParamInfo> params = functions.front()->GetParams();
	REQUIRE(params.size() == 1);
	CHECK(params.front().GetIdentifier() == "amount");
	CHECK(params.front().GetDisplayName()
		== "<parameter amount of int ReflectionTestTypes::Gadget::Hurt(int)>");
}

TEST_CASE("empty tag annotation is present but carries no data")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Gadget>();

	// The value pointer is non-null even for an empty tag: presence, not payload.
	const auto tags = type.GetAnnotations<Serializable>();
	REQUIRE(tags.size() == 1);
	CHECK(tags.front() != nullptr);
	CHECK(type.HasAnnotation<Serializable>());
}
