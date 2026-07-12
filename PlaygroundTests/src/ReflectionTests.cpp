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
	// ReSharper disable CppEnumeratorNeverUsed
	// ReSharper disable CppMemberFunctionMayBeStatic
	// ReSharper disable CppMemberFunctionMayBeConst
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

		Referencing() : Alias(Target), ConstAlias(Target) {}
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

		Tracked(const Tracked& other) : Value(other.Value) {}

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

	// A type whose own metadata names itself: a factory returning it by value, a method taking it by
	// reference, and one returning it by reference. Each self-reference needs the type's TypeInfo before
	// its construction has finished; the lazy TypeReference is what lets construction close first.
	struct Node
	{
		int Value = 0;

		Node Clone() const { return Node{Value}; }
		void CopyFrom(const Node& other) { Value = other.Value; }
		Node& Self() { return *this; }
	};

	// Two types that name each other by value: building either needs the other's TypeInfo, which needs
	// the first, a mutual cycle the lazy resolver breaks. Declared here, defined once both are complete.
	struct Pong;

	struct Ping
	{
		Pong Bounce() const;
	};

	struct Pong
	{
		Ping Bounce() const;
	};

	inline Pong Ping::Bounce() const
	{
		return Pong{};
	}
	inline Ping Pong::Bounce() const
	{
		return Ping{};
	}

	// Private member functions are reflected and invocable, the way private fields already are.
	struct WithPrivate
	{
		int Value = 0;

	private:
		int Doubled() const { return Value * 2; }
		void SetValue(int v) { Value = v; }
	};

	// A rvalue-ref-qualified overload cannot be called on the erased lvalue object the thunk holds, so it
	// reflects as metadata with no invoker (like a bitfield reflecting with no borrow). Normal overloads on
	// the same type stay invocable.
	struct RefQualified
	{
		int Value = 0;

		int OnRvalue() && { return Value; }
		int OnAny() const { return Value; }
	};

	// An enum with the default (int) underlying type. Stringifies to its enumerator name, which also
	// propagates through Palette's Primary field.
	enum class Shade
	{
		Red,
		Green
	};

	struct Palette
	{
		Shade Primary = Shade::Green;
		int Count = 2;
	};

	// A leaf with no std::format support, no trait, and no enumerators: reflectable, stringified by the
	// type-name fallback.
	struct Opaque
	{};

	// Enum with an explicit unsigned underlying type and gappy, flag-shaped values. None = 0 exercises the
	// zero-valued lookup; a bit-or'd combination (Read | Write) is a valid value no enumerator names.
	enum class Permissions : std::uint16_t
	{
		None = 0,
		Read = 1,
		Write = 2,
		Execute = 4
	};

	// Signed underlying type with a negative enumerator, to pin the two's-complement round-trip through the
	// uint64 bit pattern.
	enum class Temperature : std::int16_t
	{
		Freezing = -10,
		Boiling = 100
	};

	// Unscoped enum, to confirm the enumerator walk is not scoped-only.
	enum Direction
	{
		North,
		South,
		East,
		West
	};

	struct Base
	{
		virtual ~Base() = default;
		int V = 1;

		virtual int GetV() { return V; }
	};

	struct Child : Base
	{
		int GetV() override { return V + 1; }
	};

	struct ChildNoOverride : Base
	{};

	// Polymorphic and abstract: cannot be constructed, so its traits differ from a concrete class.
	struct AbstractShape
	{
		virtual ~AbstractShape() = default;
		virtual int Sides() const = 0;
	};

	union Scalar
	{
		int AsInt;
		float AsFloat;
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

		consteval Doc(const char* text) : Text(std::define_static_string(std::string_view{text})) {}
	};

	struct Serializable
	{};

	// One type carrying annotations on every declaration kind: the type itself, a field,
	// a member function, and a parameter. Exercises the shared Annotated surface uniformly.
	struct[[= Serializable{}]] Gadget
	{
		[[= Range{0.0, 100.0}]][[= Doc{"hit points"}]] int Health = 100;

		// The same annotation type repeated: legal, kept in declaration order, not deduplicated.
		[[= Doc{"primary"}]][[= Doc{"secondary"}]] int Nickname = 0;

		int Plain = 0;

		[[= Doc{"apply damage, return remaining health"}]] int Hurt([[= Range{0.0, 1000.0}]] int amount)
		{
			Health -= amount;
			return Health;
		}
	};

	// A struct whose fields are reflected containers: the field walk recurses into each field's facet
	// rather than the container's structure.
	struct Inventory
	{
		std::vector<int> Slots;
		std::string Owner;
	};

	// A facet defined entirely outside the reflection library. It omits Supersedes, so it adds information
	// alongside the structural view rather than replacing it.
	struct LabelFacet
	{
		std::string_view Text;
	};

	// A plain type that will be given the user facet through a TypeInfoTraits specialization, with no change
	// to the reflection core.
	struct Labeled
	{
		int Value = 1;
	};

	// ReSharper restore CppMemberFunctionMayBeConst
	// ReSharper restore CppMemberFunctionMayBeStatic
	// ReSharper restore CppParameterMayBeConst
	// ReSharper restore CppDeclaratorNeverUsed
	// ReSharper restore CppPassValueParameterByConstReference
	// ReSharper restore CppEnumeratorNeverUsed
}

using ReflectionTestTypes::AbstractShape;
using ReflectionTestTypes::Accessor;
using ReflectionTestTypes::Base;
using ReflectionTestTypes::Child;
using ReflectionTestTypes::ChildNoOverride;
using ReflectionTestTypes::Consumer;
using ReflectionTestTypes::CopyConsumer;
using ReflectionTestTypes::CopyOnlyParam;
using ReflectionTestTypes::Counter;
using ReflectionTestTypes::Direction;
using ReflectionTestTypes::Doc;
using ReflectionTestTypes::Fixed;
using ReflectionTestTypes::Gadget;
using ReflectionTestTypes::Immovable;
using ReflectionTestTypes::ImmovableHolder;
using ReflectionTestTypes::Inner;
using ReflectionTestTypes::Inventory;
using ReflectionTestTypes::Labeled;
using ReflectionTestTypes::LabelFacet;
using ReflectionTestTypes::MoveOnly;
using ReflectionTestTypes::MoveOnlyHolder;
using ReflectionTestTypes::MoveOnlyValue;
using ReflectionTestTypes::Node;
using ReflectionTestTypes::Opaque;
using ReflectionTestTypes::Outer;
using ReflectionTestTypes::Packet;
using ReflectionTestTypes::Palette;
using ReflectionTestTypes::Permissions;
using ReflectionTestTypes::Person;
using ReflectionTestTypes::Ping;
using ReflectionTestTypes::Pong;
using ReflectionTestTypes::Range;
using ReflectionTestTypes::Referencing;
using ReflectionTestTypes::RefQualified;
using ReflectionTestTypes::Scalar;
using ReflectionTestTypes::Secret;
using ReflectionTestTypes::Serializable;
using ReflectionTestTypes::Shade;
using ReflectionTestTypes::Sink;
using ReflectionTestTypes::Temperature;
using ReflectionTestTypes::Tracked;
using ReflectionTestTypes::Widget;
using ReflectionTestTypes::WithPrivate;

// A user extends the facet system entirely from outside the library: specialize TypeInfoTraits for the
// type and return the facet from MakeFacets. The reflection core assembles and keys it with no change.
template <>
struct PgE::TypeInfoTraits<Labeled> : PgE::TypeInfoTraitsDefaults
{
	static consteval auto MakeFacets() { return std::tuple{LabelFacet{.Text = "the-label"}}; }
};

TEST_CASE("reflected type name is correct")
{
	CHECK(PgE::TypeOf<int>().GetDisplayName() == "int");
}

TEST_CASE("TypeOf deduces the type from a value and matches the explicit form")
{
	constexpr Widget widget{};
	CHECK(&PgE::TypeOf(widget) == &PgE::TypeOf<Widget>());
	CHECK(&PgE::TypeOf(42) == &PgE::TypeOf<int>());
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
	const auto got = type.GetFieldValue(&person, "Age", {.Type = &PgE::TypeOf<int>(), .Data = &readBack, .IsConst = false});
	REQUIRE(got.has_value());
	CHECK(readBack == 40);

	int newAge = 41;
	const auto set = type.SetFieldValue(&person, "Age", {.Type = &PgE::TypeOf<int>(), .Data = &newAge, .IsConst = true});
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

TEST_CASE("non-formattable leaf falls back to its type name")
{
	// Opaque has no fields, no std::format support, and is not an enum, so it hits the type-name fallback.
	CHECK(PgE::ToString(Opaque{}) == "<ReflectionTestTypes::Opaque>");
}

TEST_CASE("enum field is accessible and stringifies to its enumerator name")
{
	Palette palette{};
	const PgE::TypeInfo& type = PgE::TypeOf<Palette>();

	CHECK(*type.GetFieldAs<Shade>(&palette, "Primary") == Shade::Green);
	REQUIRE(type.SetFieldAs(&palette, "Primary", Shade::Red).has_value());
	CHECK(palette.Primary == Shade::Red);

	// The enum name propagates through the owning struct's field walk.
	CHECK(PgE::ToString(palette) == "{Primary: Red, Count: 2}");
}

TEST_CASE("enumeration facet exposes enumerators, values, and underlying type")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Permissions>();
	REQUIRE(type.GetKind() == PgE::TypeKind::Enum);

	const PgE::EnumerationFacet* enumeration = type.GetFacet<PgE::EnumerationFacet>();
	REQUIRE(enumeration != nullptr);

	// The underlying type is reflected as the same TypeInfo instance TypeOf<uint16_t> yields.
	const PgE::TypeInfo& underlying = enumeration->GetUnderlyingType();
	CHECK(&underlying == &PgE::TypeOf<std::uint16_t>());
	CHECK(underlying.GetKind() == PgE::TypeKind::Integral);
	CHECK(underlying.GetSize() == sizeof(std::uint16_t));

	// Enumerators are listed in declaration order, each with its value.
	const auto enumerators = enumeration->GetEnumerators();
	REQUIRE(enumerators.size() == 4);
	CHECK(enumerators[0].GetIdentifier() == "None");
	CHECK(enumerators[0].GetValue() == 0);
	CHECK(enumerators[1].GetIdentifier() == "Read");
	CHECK(enumerators[1].GetValue() == 1);
	CHECK(enumerators[3].GetIdentifier() == "Execute");
	CHECK(enumerators[3].GetValue() == 4);

	// Lookups resolve both directions; a miss is a null result, not a crash.
	CHECK(enumeration->FindByIdentifier("Write")->GetValue() == 2);
	CHECK(enumeration->FindByValue(4)->GetIdentifier() == "Execute");
	CHECK(enumeration->FindByIdentifier("Delete") == nullptr);
	CHECK(enumeration->FindByValue(99) == nullptr);
}

TEST_CASE("non-enum types carry no enumeration facet")
{
	CHECK(PgE::TypeOf<int>().GetFacet<PgE::EnumerationFacet>() == nullptr);
	CHECK(PgE::TypeOf<Palette>().GetFacet<PgE::EnumerationFacet>() == nullptr);
}

TEST_CASE("enum name round-trips through the typed sugar")
{
	CHECK(PgE::EnumToName(Permissions::Write) == "Write");
	CHECK(PgE::EnumFromName<Permissions>("Execute") == Permissions::Execute);

	// A bit-or'd value no enumerator names has no exact name, but still renders numerically.
	constexpr auto combined =
		static_cast<Permissions>(static_cast<std::uint16_t>(Permissions::Read) | static_cast<std::uint16_t>(Permissions::Write));
	CHECK(PgE::EnumToName(combined) == std::nullopt);
	CHECK(PgE::ToString(combined) == "3");

	// An unknown name does not parse.
	CHECK(PgE::EnumFromName<Permissions>("Sudo") == std::nullopt);
}

TEST_CASE("signed enumerator round-trips through the uint64 bit pattern")
{
	const PgE::EnumerationFacet* enumeration = PgE::TypeOf<Temperature>().GetFacet<PgE::EnumerationFacet>();
	REQUIRE(enumeration != nullptr);
	CHECK(&enumeration->GetUnderlyingType() == &PgE::TypeOf<std::int16_t>());

	// -10 as int16 widened to uint64 is its two's-complement bit pattern; the stored value matches, and a
	// lookup by that pattern recovers the name.
	constexpr auto freezingBits = static_cast<std::uint64_t>(static_cast<std::int16_t>(-10));
	CHECK(enumeration->FindByIdentifier("Freezing")->GetValue() == freezingBits);
	CHECK(enumeration->FindByValue(freezingBits)->GetIdentifier() == "Freezing");

	CHECK(PgE::EnumToName(Temperature::Freezing) == "Freezing");
	CHECK(PgE::EnumFromName<Temperature>("Freezing") == Temperature::Freezing);
	CHECK(PgE::ToString(Temperature::Freezing) == "Freezing");

	// An unnamed negative value falls back to its signed number, not the raw uint64 bits.
	CHECK(PgE::ToString(static_cast<Temperature>(-5)) == "-5");
}

TEST_CASE("unscoped enum is reflected like a scoped one")
{
	const PgE::EnumerationFacet* enumeration = PgE::TypeOf<Direction>().GetFacet<PgE::EnumerationFacet>();
	REQUIRE(enumeration != nullptr);
	CHECK(enumeration->GetEnumerators().size() == 4);
	CHECK(PgE::EnumToName(Direction::East) == "East");
	CHECK(PgE::ToString(Direction::West) == "West");
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
	const PgE::FunctionInfo* hurt = functions.front();

	const auto functionDocs = hurt->GetAnnotations<Doc>();
	REQUIRE(functionDocs.size() == 1);
	CHECK(std::string_view{functionDocs.front()->Text} == "apply damage, return remaining health");

	// Parameter-level annotation.
	const std::span<const PgE::ParameterInfo> params = hurt->GetParams();
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

	const std::span<const PgE::ParameterInfo> params = functions.front()->GetParams();
	REQUIRE(params.size() == 1);
	CHECK(params.front().GetIdentifier() == "amount");
	CHECK(params.front().GetDisplayName() == "<parameter amount of int ReflectionTestTypes::Gadget::Hurt(int)>");
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

TEST_CASE("type layout reports size and alignment")
{
	CHECK(PgE::TypeOf<int>().GetSize() == sizeof(int));
	CHECK(PgE::TypeOf<int>().GetAlignment() == alignof(int));

	CHECK(PgE::TypeOf<Widget>().GetSize() == sizeof(Widget));
	CHECK(PgE::TypeOf<Widget>().GetAlignment() == alignof(Widget));

	CHECK(PgE::TypeOf<int[4]>().GetSize() == sizeof(int[4]));

	// void has no object representation, so it reports a zero layout instead of failing to reflect.
	CHECK(PgE::TypeOf<void>().GetSize() == 0);
	CHECK(PgE::TypeOf<void>().GetAlignment() == 0);
}

TEST_CASE("type kind classifies every category")
{
	CHECK(PgE::TypeOf<void>().GetKind() == PgE::TypeKind::Void);
	CHECK(PgE::TypeOf<std::nullptr_t>().GetKind() == PgE::TypeKind::NullPointer);
	CHECK(PgE::TypeOf<int>().GetKind() == PgE::TypeKind::Integral);
	CHECK(PgE::TypeOf<bool>().GetKind() == PgE::TypeKind::Integral);
	CHECK(PgE::TypeOf<double>().GetKind() == PgE::TypeKind::FloatingPoint);
	CHECK(PgE::TypeOf<Shade>().GetKind() == PgE::TypeKind::Enum);
	CHECK(PgE::TypeOf<Scalar>().GetKind() == PgE::TypeKind::Union);
	CHECK(PgE::TypeOf<Widget>().GetKind() == PgE::TypeKind::Class);
	CHECK(PgE::TypeOf<int[4]>().GetKind() == PgE::TypeKind::Array);
	CHECK(PgE::TypeOf<int*>().GetKind() == PgE::TypeKind::Pointer);
	CHECK(PgE::TypeOf<int Widget::*>().GetKind() == PgE::TypeKind::MemberObjectPointer);
	CHECK(PgE::TypeOf<int (Widget::*)() const>().GetKind() == PgE::TypeKind::MemberFunctionPointer);
	CHECK(PgE::TypeOf<int(int)>().GetKind() == PgE::TypeKind::Function);
	CHECK(PgE::TypeOf<int&>().GetKind() == PgE::TypeKind::LValueReference);
	CHECK(PgE::TypeOf<int&&>().GetKind() == PgE::TypeKind::RValueReference);
}

TEST_CASE("union members are reflected and accessible")
{
	Scalar scalar{};
	const PgE::TypeInfo& type = PgE::TypeOf<Scalar>();

	const PgE::FieldInfo* asInt = type.FindFieldByIdentifier("AsInt");
	REQUIRE(asInt != nullptr);
	CHECK(&asInt->GetTypeInfo() == &PgE::TypeOf<int>());

	// Each field reads the member that was last written (the active one), so access is well-defined.
	REQUIRE(type.SetFieldAs(&scalar, "AsInt", 7).has_value());
	CHECK(*type.GetFieldAs<int>(&scalar, "AsInt") == 7);

	REQUIRE(type.SetFieldAs(&scalar, "AsFloat", 1.5f).has_value());
	CHECK(*type.GetFieldAs<float>(&scalar, "AsFloat") == 1.5f);
}

TEST_CASE("a function type reflects as a layout-less Function")
{
	// A bare function type has no object representation: it reflects with a zero layout and none of
	// the object-property traits, so reflecting one is well-formed rather than a hard error.
	const PgE::TypeInfo& type = PgE::TypeOf<int(double, char)>();
	const PgE::TypeTraits& traits = type.GetTraits();

	CHECK(traits.Kind == PgE::TypeKind::Function);
	CHECK(traits.Size == 0);
	CHECK(traits.Alignment == 0);
	CHECK_FALSE(traits.IsTriviallyCopyable);
	CHECK_FALSE(traits.IsDefaultConstructible);
}

TEST_CASE("trait predicates read from the type system")
{
	const PgE::TypeTraits& widget = PgE::TypeOf<Widget>().GetTraits();
	CHECK(widget.IsTriviallyCopyable);
	CHECK(widget.IsDefaultConstructible);
	CHECK_FALSE(widget.IsPolymorphic);
	CHECK_FALSE(widget.IsAbstract);

	// A std::string member makes Person non-blittable.
	CHECK_FALSE(PgE::TypeOf<Person>().GetTraits().IsTriviallyCopyable);

	// Signedness is meaningful only for arithmetic types.
	CHECK(PgE::TypeOf<int>().GetTraits().IsSigned);
	CHECK_FALSE(PgE::TypeOf<unsigned>().GetTraits().IsSigned);
	CHECK_FALSE(PgE::TypeOf<Widget>().GetTraits().IsSigned);

	const PgE::TypeTraits& base = PgE::TypeOf<Base>().GetTraits();
	CHECK(base.IsPolymorphic);
	CHECK_FALSE(base.IsAbstract);

	const PgE::TypeTraits& shape = PgE::TypeOf<AbstractShape>().GetTraits();
	CHECK(shape.IsPolymorphic);
	CHECK(shape.IsAbstract);
	CHECK_FALSE(shape.IsDefaultConstructible);

	// Doc declares only a consteval converting constructor, so it is not default constructible.
	CHECK_FALSE(PgE::TypeOf<Doc>().GetTraits().IsDefaultConstructible);
}

TEST_CASE("raw-storage lifecycle traits")
{
	// A plain-int aggregate: constructible, droppable, and comparable purely as bytes.
	const PgE::TypeTraits& widget = PgE::TypeOf<Widget>().GetTraits();
	CHECK(widget.IsTriviallyDefaultConstructible);
	CHECK(widget.IsTriviallyDestructible);
	CHECK(widget.IsStandardLayout);
	CHECK(widget.HasUniqueObjectRepresentations);

	// A std::string member pulls in a destructor and a non-trivial default constructor.
	const PgE::TypeTraits& person = PgE::TypeOf<Person>().GetTraits();
	CHECK_FALSE(person.IsTriviallyDefaultConstructible);
	CHECK_FALSE(person.IsTriviallyDestructible);

	// A default member initializer keeps the type default constructible but not trivially so.
	const PgE::TypeTraits& counter = PgE::TypeOf<Counter>().GetTraits();
	CHECK(counter.IsDefaultConstructible);
	CHECK_FALSE(counter.IsTriviallyDefaultConstructible);

	// A polymorphic type is never standard layout (the vtable pointer breaks it).
	CHECK_FALSE(PgE::TypeOf<AbstractShape>().GetTraits().IsStandardLayout);
}

TEST_CASE("construction, polymorphism, and shape traits")
{
	const PgE::TypeTraits& widget = PgE::TypeOf<Widget>().GetTraits();
	CHECK(widget.IsAggregate);
	CHECK_FALSE(widget.HasVirtualDestructor);
	CHECK_FALSE(widget.IsEmpty);

	// Base declares a virtual destructor; that is what makes owning deletion through it safe.
	CHECK(PgE::TypeOf<Base>().GetTraits().HasVirtualDestructor);

	// A user-provided constructor disqualifies aggregate-ness.
	CHECK_FALSE(PgE::TypeOf<Doc>().GetTraits().IsAggregate);

	// An empty tag type.
	CHECK(PgE::TypeOf<Serializable>().GetTraits().IsEmpty);
}

TEST_CASE("template-instance and scoped-enum traits")
{
	// std::string is a template specialization; a plain struct is not.
	CHECK(PgE::TypeOf<std::string>().GetTraits().IsTemplateInstance);
	CHECK(PgE::TypeOf<int>().GetTraits().IsTemplateInstance == false);
	CHECK_FALSE(PgE::TypeOf<Widget>().GetTraits().IsTemplateInstance);

	// Shade is an enum class; the flag is meaningful only for enums.
	CHECK(PgE::TypeOf<Shade>().GetTraits().IsScopedEnum);
	CHECK_FALSE(PgE::TypeOf<int>().GetTraits().IsScopedEnum);
}

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
