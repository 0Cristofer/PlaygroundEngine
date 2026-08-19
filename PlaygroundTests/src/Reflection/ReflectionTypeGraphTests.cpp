#include <doctest/doctest.h>

import PlaygroundEngine.Reflection;
import PlaygroundTests.ReflectionTestTypes;

import std;

using namespace ReflectionTestTypes;

namespace
{
	// ReSharper disable CppDeclaratorNeverUsed
	// ReSharper disable CppParameterMayBeConst
	// ReSharper disable CppPassValueParameterByConstReference
	// ReSharper disable CppEnumeratorNeverUsed
	// ReSharper disable CppMemberFunctionMayBeStatic
	// ReSharper disable CppMemberFunctionMayBeConst

	// Defined in this normal TU, not in the ReflectionTestTypes module: annotating a base specifier ICEs
	// GCC 16's module writer (see docs/ReflectionInternals.md), but compiles fine in a non-module TU.
	struct AnnotatedBase : [[= Serializable{}]] public RootA
	{
		int Own = 0;
	};

	// ReSharper restore CppMemberFunctionMayBeConst
	// ReSharper restore CppMemberFunctionMayBeStatic
	// ReSharper restore CppParameterMayBeConst
	// ReSharper restore CppDeclaratorNeverUsed
	// ReSharper restore CppPassValueParameterByConstReference
	// ReSharper restore CppEnumeratorNeverUsed
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
	const auto cloned = clone->InvokeAs<Node>(original);
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

TEST_CASE("a type exposes its direct base classes in declaration order")
{
	const std::span<const PgE::BaseInfo> bases = PgE::TypeOf<MultiDerived>().GetBases();
	REQUIRE(bases.size() == 3);

	// Each base resolves to the same canonical TypeInfo instance a caller reaches through TypeOf.
	CHECK(&bases[0].GetTypeInfo() == &PgE::TypeOf<RootA>());
	CHECK(&bases[1].GetTypeInfo() == &PgE::TypeOf<RootB>());
	CHECK(&bases[2].GetTypeInfo() == &PgE::TypeOf<RootC>());
}

TEST_CASE("a type with no base classes has no reflected bases")
{
	CHECK(PgE::TypeOf<RootA>().GetBases().empty());
}

TEST_CASE("base access specifiers are reflected")
{
	const std::span<const PgE::BaseInfo> bases = PgE::TypeOf<MultiDerived>().GetBases();
	CHECK(bases[0].GetAccess() == PgE::AccessKind::Public);
	CHECK(bases[1].GetAccess() == PgE::AccessKind::Protected);
	CHECK(bases[2].GetAccess() == PgE::AccessKind::Private);
}

TEST_CASE("base subobject offsets are reflected")
{
	const std::span<const PgE::BaseInfo> bases = PgE::TypeOf<MultiDerived>().GetBases();

	// Three single-int bases lay out contiguously: the first at the start, the rest offset past it.
	CHECK(bases[0].GetOffset() == 0);
	CHECK(bases[1].GetOffset() == sizeof(RootA));
	CHECK(bases[2].GetOffset() == sizeof(RootA) + sizeof(RootB));

	// The reflected offset matches the compiler's actual upcast for an accessible (public) base.
	TwoPublic obj;
	const auto actualB = reinterpret_cast<const std::byte*>(static_cast<const RootB*>(&obj)) - reinterpret_cast<const std::byte*>(&obj);
	CHECK(PgE::TypeOf<TwoPublic>().GetBases()[1].GetOffset() == static_cast<std::size_t>(actualB));
}

TEST_CASE("reflected bases are direct only, inherited bases are reached by recursion")
{
	const std::span<const PgE::BaseInfo> bases = PgE::TypeOf<Grandchild>().GetBases();
	REQUIRE(bases.size() == 1);
	CHECK(&bases[0].GetTypeInfo() == &PgE::TypeOf<MultiDerived>());

	// The grandparents are not flattened in; they are reached by walking the base's own bases.
	CHECK(bases[0].GetTypeInfo().GetBases().size() == 3);
}

TEST_CASE("annotations on a base specifier are reflected")
{
	const std::span<const PgE::BaseInfo> bases = PgE::TypeOf<AnnotatedBase>().GetBases();
	REQUIRE(bases.size() == 1);
	REQUIRE(bases[0].GetAnnotations().size() == 1);
	CHECK(&bases[0].GetAnnotations()[0].Type.Get() == &PgE::TypeOf<Serializable>());
}

TEST_CASE("upcasting a borrow reaches the base subobject and carries its constness")
{
	TwoPublic derived;
	const std::span<const PgE::BaseInfo> bases = PgE::TypeOf<TwoPublic>().GetBases();
	REQUIRE(bases.size() == 2);

	const PgE::TypedRef secondBase = bases[1].Upcast(PgE::TypedRefOf(derived));
	CHECK(secondBase.Type == &PgE::TypeOf<RootB>());
	CHECK(secondBase.Data == static_cast<RootB*>(&derived));
	CHECK_FALSE(secondBase.IsConst);

	// A base subobject inside someone else's object is a borrow, never an offer to move out of it.
	CHECK_FALSE(secondBase.Movable);

	const TwoPublic& constant = derived;
	CHECK(bases[1].Upcast(PgE::TypedRefOf(constant)).IsConst);
}

TEST_CASE("an inherited field is read through the upcast borrow, not the derived pointer")
{
	TwoPublic derived;
	derived.B = 42;

	const PgE::BaseInfo& base = PgE::TypeOf<TwoPublic>().GetBases()[1];
	const PgE::TypedRef baseObject = base.Upcast(PgE::TypedRefOf(derived));

	// RootB sits at a nonzero offset, so reading its field through the derived address would read RootA's.
	const PgE::FieldInfo* field = baseObject.Type->FindFieldByIdentifier("B");
	REQUIRE(field != nullptr);
	CHECK(&field->GetDeclaringType() == &PgE::TypeOf<RootB>());

	const auto borrowed = field->GetRef(baseObject);
	REQUIRE(borrowed.has_value());
	CHECK(*static_cast<const int*>(borrowed->Data) == 42);
}

TEST_CASE("a field rejects an object that is not its declaring type")
{
	TwoPublic derived;
	const PgE::FieldInfo* field = PgE::TypeOf<RootB>().FindFieldByIdentifier("B");
	REQUIRE(field != nullptr);

	// The thunk indexes from RootB's layout, so handing it the derived object would read at the wrong
	// offset. Without the tag there is nothing to catch it.
	const auto wrongObject = field->GetRef(PgE::TypedRefOf(derived));
	REQUIRE_FALSE(wrongObject.has_value());
	CHECK(wrongObject.error().Reason == PgE::FieldError::ObjectTypeMismatch);

	CHECK(field->GetAs<int>(derived).error().Reason == PgE::FieldError::ObjectTypeMismatch);
}

TEST_CASE("a read-only object borrow makes every field borrow below it read-only")
{
	TwoPublic derived;
	const PgE::FieldInfo* field = PgE::TypeOf<TwoPublic>().FindFieldByIdentifier("Own");
	REQUIRE(field != nullptr);

	// Own is a plain mutable field: the constness here comes from the object, which is the propagation the
	// erased object parameter exists for.
	const TwoPublic& constant = derived;
	const auto readOnly = field->GetRef(PgE::TypedRefOf(constant));
	REQUIRE(readOnly.has_value());
	CHECK(readOnly->IsConst);

	CHECK(field->GetRef(PgE::TypedRefOf(derived))->IsConst == false);

	int replacement = 5;
	const auto rejected = field->SetValue(PgE::TypedRefOf(constant), PgE::TypedRefOf(replacement));
	REQUIRE_FALSE(rejected.has_value());
	CHECK(rejected.error().Reason == PgE::FieldError::ConstViolation);
}

TEST_CASE("dereferencing a borrow yields the pointee, peeled and with the pointee's own constness")
{
	int value = 7;
	int* pointer = &value;

	const auto pointee = PgE::TypedRefOf(pointer).Dereference();
	REQUIRE(pointee.has_value());
	CHECK(pointee->Type == &PgE::TypeOf<int>());
	CHECK(pointee->Data == &value);
	CHECK_FALSE(pointee->IsConst);
	CHECK_FALSE(pointee->Movable);
}

TEST_CASE("const stops at the pointer, exactly as the language says")
{
	int value = 7;

	// A pointer TO const: the pointee is read-only, and the cv node is peeled off the borrow's type.
	const int* toConst = &value;
	const auto throughPointerToConst = PgE::TypedRefOf(toConst).Dereference();
	REQUIRE(throughPointerToConst.has_value());
	CHECK(throughPointerToConst->Type == &PgE::TypeOf<int>());
	CHECK(throughPointerToConst->IsConst);

	// A const pointer to a mutable int: the pointer cannot be repointed, but *p = x is legal, so the
	// borrow's own constness must not reach the pointee.
	int* const constPointer = &value;
	const auto throughConstPointer = PgE::TypedRefOf(constPointer).Dereference();
	REQUIRE(throughConstPointer.has_value());
	CHECK_FALSE(throughConstPointer->IsConst);

	// Both, for completeness: only the pointee's const counts, and here it is present.
	const int* const both = &value;
	CHECK(PgE::TypedRefOf(both).Dereference()->IsConst);
}

TEST_CASE("dereference reports what it cannot do")
{
	int* nullPointer = nullptr;
	CHECK(PgE::TypedRefOf(nullPointer).Dereference().error().Reason == PgE::DereferenceError::NullPointer);

	int value = 7;
	CHECK(PgE::TypedRefOf(value).Dereference().error().Reason == PgE::DereferenceError::NotAPointer);
	CHECK(PgE::TypedRef{}.Dereference().error().Reason == PgE::DereferenceError::NotAPointer);

	// A function pointer is a pointer, but names no object to borrow. What it points at is decided before
	// its value is read, so a null one says that rather than posing as an expandable null.
	int (*function)(int) = nullptr;
	CHECK(PgE::TypedRefOf(function).Dereference().error().Reason == PgE::DereferenceError::NotAnObjectPointer);
	function = [](const int x) { return x; };
	CHECK(PgE::TypedRefOf(function).Dereference().error().Reason == PgE::DereferenceError::NotAnObjectPointer);

	// A borrow with no object at all is not a null pointer: there is no pointer to have read.
	CHECK(PgE::TypedRef{.Type = &PgE::TypeOf<int*>()}.Dereference().error().Reason == PgE::DereferenceError::NullObject);
}
