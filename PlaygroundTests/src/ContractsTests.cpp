#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Reflection;

namespace ContractTestTypes
{
	struct Packed
	{
		int A;
		int B;
	};

	struct Padded
	{
		char C;
		int I;
	};

	struct WithTailPadding
	{
		Packed P;
		char C;
	};
}

using namespace PgE;
using ContractTestTypes::Packed;
using ContractTestTypes::Padded;
using ContractTestTypes::WithTailPadding;

// The contracts as they are meant to be used: asserted at compile time, so a regression in the
// predicate logic or a change to one of these types fails the build rather than a test.
static_assert(IsTriviallyReplicable<Packed>());
static_assert(IsTriviallyReplicable<int>());
static_assert(HasNoPadding<Packed>());
static_assert(!HasNoPadding<Padded>());
static_assert(!HasNoPadding<WithTailPadding>());
static_assert(FitsBudget<Packed, 8>());
static_assert(!FitsBudget<Packed, 4>());

TEST_CASE("static-contract predicates classify types by machine shape")
{
	CHECK(IsTriviallyReplicable<Packed>());
	CHECK(HasNoPadding<Packed>());
	CHECK_FALSE(HasNoPadding<Padded>());
	CHECK_FALSE(HasNoPadding<WithTailPadding>());
	CHECK(FitsBudget<Packed, 8>());
	CHECK_FALSE(FitsBudget<Packed, 4>());
}
