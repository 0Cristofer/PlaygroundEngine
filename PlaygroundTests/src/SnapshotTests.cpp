#include <doctest/doctest.h>

import std;
import PlaygroundTests.SnapshotHarness;

// A named namespace so the reflected identifiers are deterministic across runs, matching the
// convention in ReflectionTests.cpp.
namespace SnapshotTestTypes
{
	struct Vector2
	{
		int X;
		int Y;
	};

	struct Base
	{
		int A;
	};

	struct Derived : public Base
	{
		int A;
	};
}

TEST_CASE("DescribeType snapshot pins a struct's reflected shape")
{
	// A characterization test: the golden captures the current reflected shape and layout, and any
	// change to either surfaces as a diff that must be consciously re-blessed (PGE_BLESS=1). A diff on
	// a toolchain upgrade is expected, not an alarm.
	const std::string describedVector = PgE::Snapshot::DescribeType<SnapshotTestTypes::Vector2>();
	const PgE::Snapshot::SnapshotResult resultVector = PgE::Snapshot::CheckSnapshot("Vector2", describedVector);
	CHECK_MESSAGE(resultVector.Matched, resultVector.Detail);

	const std::string describedDerived = PgE::Snapshot::DescribeType<SnapshotTestTypes::Derived>();
	const PgE::Snapshot::SnapshotResult resultDerived = PgE::Snapshot::CheckSnapshot("Derived", describedDerived);
	CHECK_MESSAGE(resultDerived.Matched, resultDerived.Detail);
}
