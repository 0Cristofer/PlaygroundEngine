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
