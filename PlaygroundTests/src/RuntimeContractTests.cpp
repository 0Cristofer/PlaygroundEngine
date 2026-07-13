#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Diagnostics;
import PlaygroundTests.ContractSeam;

namespace
{
	int Doubled(int value)
		pre(value > 0)
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

// The violation-report kind text the runtime handler logs, pinned so the label a diagnostic surfaces
// stays stable per assertion kind.
TEST_CASE("contract kind text maps each assertion kind")
{
	CHECK(PgE::ContractKindText(std::contracts::assertion_kind::pre) == "pre");
	CHECK(PgE::ContractKindText(std::contracts::assertion_kind::post) == "post");
	CHECK(PgE::ContractKindText(std::contracts::assertion_kind::assert) == "assert");
}
