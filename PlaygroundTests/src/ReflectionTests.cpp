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
	// ReSharper restore CppParameterMayBeConst
	// ReSharper restore CppDeclaratorNeverUsed
}

using ReflectionTestTypes::Person;
using ReflectionTestTypes::Widget;
using ReflectionTestTypes::Counter;
using ReflectionTestTypes::Accessor;
using ReflectionTestTypes::MoveOnly;
using ReflectionTestTypes::Sink;

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

// Invocation: locate a reflected function by name and call it through the erased ABI, unpacking
// the boxed return with the typed sugar.
TEST_CASE("reflected member function invokes and returns")
{
    Widget widget{3, 4};

    const std::vector<const PgE::FuncInfo*> areas = PgE::TypeOf<Widget>().FindFunctionsByName("Area");
    REQUIRE(areas.size() == 1);

    const auto result = areas.front()->InvokeAs<int>(&widget);

    REQUIRE(result.has_value());
    CHECK(*result == 12);
}

// A void, argument-taking call mutates the borrowed instance in place.
TEST_CASE("reflected member function mutates its instance")
{
    Widget widget{1, 1};

    const PgE::FuncInfo* resize = PgE::TypeOf<Widget>().FindFunctionsByName("Resize").front();
    const auto result = resize->InvokeAs(&widget, 2, 5);

    REQUIRE(result.has_value());
    CHECK(widget.Width == 2);
    CHECK(widget.Height == 5);
}

// The erased boundary rejects malformed calls instead of corrupting memory: wrong arity and a
// mistyped argument both surface as errors, the latter carrying the offending argument index.
TEST_CASE("invoke reports arity and type mismatches")
{
    Widget widget{};
    const PgE::FuncInfo* resize = PgE::TypeOf<Widget>().FindFunctionsByName("Resize").front();

    int width = 2;
    const PgE::TypedRef tooFew[] = {{&PgE::TypeOf<int>(), &width, false}};
    const auto arity = resize->Invoke(&widget, tooFew);
    REQUIRE_FALSE(arity.has_value());
    CHECK(arity.error().Reason == PgE::InvokeError::ArityMismatch);

    int height = 5;
    std::string wrong = "x";
    const PgE::TypedRef mistyped[] = {{&PgE::TypeOf<std::string>(), &wrong, false},
                                      {&PgE::TypeOf<int>(), &height, false}};
    const auto type = resize->Invoke(&widget, mistyped);
    REQUIRE_FALSE(type.has_value());
    CHECK(type.error().Reason == PgE::InvokeError::TypeMismatch);
    CHECK(type.error().ArgumentIndex == 0);
}

// Object constness routes through overload resolution: a const instance can only reach the const
// Invoke overload, which runs const-qualified functions and refuses mutating ones without calling.
TEST_CASE("const object reaches only const-callable functions")
{
    Counter counter{7};
    const Counter& readOnly = counter;
    const PgE::TypeInfo& type = PgE::TypeOf<Counter>();

    const auto value = type.FindFunctionsByName("Get").front()->InvokeAs<int>(&readOnly);
    REQUIRE(value.has_value());
    CHECK(*value == 7);

    const auto mutated = type.FindFunctionsByName("Add").front()->InvokeAs(&readOnly, 1);
    REQUIRE_FALSE(mutated.has_value());
    CHECK(mutated.error().Reason == PgE::InvokeError::ConstViolation);
    CHECK(counter.Value == 7);
}

// A reference-returning function is erased as a pointer to the referent; the typed sugar hands back
// a std::reference_wrapper that converts to the live reference (mutable and const alike).
TEST_CASE("reflected function returning a reference yields a live reference")
{
    Accessor accessor{};

    const auto mutableRef = PgE::TypeOf<Accessor>().FindFunctionsByName("Mutable").front()->InvokeAs<int&>(&accessor);
    REQUIRE(mutableRef.has_value());
    int& reference = mutableRef.value();
    reference = 42;
    CHECK(accessor.Value == 42);

    const auto view = PgE::TypeOf<Accessor>().FindFunctionsByName("Readonly").front()->InvokeAs<const int&>(&accessor);
    REQUIRE(view.has_value());
    CHECK(view.value() == 42);
}

// Requesting a return from a void function is rejected, not read out of uninitialized storage, and
// the function is not called.
TEST_CASE("value-return sugar on a void function reports ReturnTypeMismatch")
{
    Widget widget{};
    const PgE::FuncInfo* resize = PgE::TypeOf<Widget>().FindFunctionsByName("Resize").front();

    const auto result = resize->InvokeAs<int>(&widget, 2, 5);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().Reason == PgE::InvokeError::ReturnTypeMismatch);
    CHECK(widget.Width == 0);
}

// The const guard covers move bindings: a const-tagged argument for a by-value move-only parameter
// is rejected instead of being silently moved-from; a mutable one is accepted and moved.
TEST_CASE("const argument to a move-only parameter is rejected")
{
    Sink sink{};
    const PgE::FuncInfo* consume = PgE::TypeOf<Sink>().FindFunctionsByName("Consume").front();

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
