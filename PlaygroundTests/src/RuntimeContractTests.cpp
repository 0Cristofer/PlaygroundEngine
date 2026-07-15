#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Diagnostics;
import PlaygroundEngine.Reflection;
import PlaygroundTests.ContractSeam;

namespace
{
	int Doubled(int value) pre(value > 0)
	{
		return value * 2;
	}

	int IncrementNonNegative(int value)
	{
		contract_assert(value >= 0);
		return value + 1;
	}
}

// Runtime contracts as tests exercise them: a satisfied contract runs the body, a violated one is
// rejected before the body via the throwing test seam (ContractSeam.cpp), catchable by doctest.
TEST_CASE("a satisfied precondition runs the body")
{
	CHECK(Doubled(3) == 6);
}

TEST_CASE("a violated precondition is rejected before the body runs")
{
	CHECK_THROWS_AS(Doubled(0), PgETest::ContractViolationError);
	CHECK_THROWS_AS(Doubled(-1), PgETest::ContractViolationError);
}

TEST_CASE("a violated contract_assert is rejected")
{
	CHECK(IncrementNonNegative(4) == 5);
	CHECK_THROWS_AS(IncrementNonNegative(-1), PgETest::ContractViolationError);
}

// The reflection type model's own preconditions, through the same seam. Each guards a call that would
// otherwise dereference a null thunk or a null object, which CanDestroy/CanStringify only advise against.
TEST_CASE("a thunk-backed reflection accessor rejects a call it cannot serve")
{
	// A non-null address the absent thunk never dereferences, so the null-thunk arm is isolated from the
	// null-object one. What each accessor does when it can serve the call is covered by the reflection tests.
	int value = 7;

	// void reflects with no destroy thunk; int has one but still needs an object to run it on.
	CHECK_THROWS_AS(PgE::TypeOf<void>().Destroy(&value), PgETest::ContractViolationError);
	CHECK_THROWS_AS(PgE::TypeOf<int>().Destroy(nullptr), PgETest::ContractViolationError);

	CHECK_THROWS_AS(PgE::TypeOf<int>().Stringify(nullptr), PgETest::ContractViolationError);
}

TEST_CASE("a type reference with no resolver rejects the read")
{
	CHECK_THROWS_AS(PgE::TypeReference{}.Get(), PgETest::ContractViolationError);
}

// The violation-report kind text the runtime handler logs, pinned so the label a diagnostic surfaces
// stays stable per assertion kind.
TEST_CASE("contract kind text maps each assertion kind")
{
	CHECK(PgE::ContractKindText(std::contracts::assertion_kind::pre) == "pre");
	CHECK(PgE::ContractKindText(std::contracts::assertion_kind::post) == "post");
	CHECK(PgE::ContractKindText(std::contracts::assertion_kind::assert) == "assert");
}
