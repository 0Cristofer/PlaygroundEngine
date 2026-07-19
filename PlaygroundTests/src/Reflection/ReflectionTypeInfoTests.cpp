#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Reflection;
import PlaygroundTests.ReflectionTestTypes;

using namespace ReflectionTestTypes;

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

TEST_CASE("identifier and display name are exposed separately on every declaration kind")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Gadget>();
	CHECK(type.GetIdentifier() == "Gadget");

	// The fixtures live in an importable module, so display names carry GCC's module-ownership suffix
	// ("@PlaygroundTests.ReflectionTestTypes"); the identifier stays the plain source spelling.
	CHECK(type.GetDisplayName() == "ReflectionTestTypes::Gadget@PlaygroundTests.ReflectionTestTypes");

	// A fundamental type has no identifier in the language (has_identifier(^^int) is false), so it is named
	// from structure instead: the identifier is the query key, and it would otherwise be a hole on the most
	// common types there are.
	CHECK(PgE::TypeOf<int>().GetIdentifier() == "int32");
	CHECK(PgE::TypeOf<int>().GetDisplayName() == "int");
	CHECK(PgE::TypeOf<unsigned long long>().GetIdentifier() == "uint64");
	CHECK(PgE::TypeOf<double>().GetIdentifier() == "float64");

	// char is named by spelling, not by size and signedness, which would collapse it with signed char and
	// int8_t: it is the string element type, and StringFacet must stay distinguishable from a byte buffer.
	CHECK(PgE::TypeOf<char>().GetIdentifier() == "char");
	CHECK(PgE::TypeOf<signed char>().GetIdentifier() == "int8");
	CHECK(PgE::TypeOf<unsigned char>().GetIdentifier() == "uint8");

	// A template specialization has no identifier: it is named by its template plus its arguments. Aliases
	// like std::string dissolve during template deduction, so TypeOf sees the specialization.
	CHECK(PgE::TypeOf<std::string>().GetIdentifier().empty());

	// A compound type has no identifier either: it is named by decomposition.
	CHECK(PgE::TypeOf<int*>().GetIdentifier().empty());
	CHECK(PgE::TypeOf<int*>().GetDisplayName() == "int*");
	CHECK(PgE::TypeOf<int[4]>().GetIdentifier().empty());

	const PgE::FieldInfo* health = type.FindFieldByIdentifier("Health");
	REQUIRE(health != nullptr);
	CHECK(health->GetIdentifier() == "Health");
	CHECK(health->GetDisplayName() == "ReflectionTestTypes::Gadget@PlaygroundTests.ReflectionTestTypes::Health");

	const auto functions = type.FindFunctionsByIdentifier("Hurt");
	REQUIRE(functions.size() == 1);
	CHECK(functions.front()->GetIdentifier() == "Hurt");
	CHECK(functions.front()->GetDisplayName() == "int ReflectionTestTypes::Gadget@PlaygroundTests.ReflectionTestTypes::Hurt(int)");

	const std::span<const PgE::ParameterInfo> params = functions.front()->GetParams();
	REQUIRE(params.size() == 1);
	CHECK(params.front().GetIdentifier() == "amount");
	CHECK(params.front().GetDisplayName() == "<parameter amount of int ReflectionTestTypes::Gadget@PlaygroundTests.ReflectionTestTypes::Hurt(int)>");
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
