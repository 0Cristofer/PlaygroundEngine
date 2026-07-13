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
}

TEST_CASE("DescribeType snapshot pins a struct's reflected shape")
{
	// A characterization test: the golden captures the current reflected shape and layout, and any
	// change to either surfaces as a diff that must be consciously re-blessed (PGE_BLESS=1). A diff on
	// a toolchain upgrade is expected, not an alarm.
	const std::string described = PgE::Snapshot::DescribeType<SnapshotTestTypes::Vector2>();
	const PgE::Snapshot::SnapshotResult result = PgE::Snapshot::CheckSnapshot("Vector2", described);

	CHECK_MESSAGE(result.Matched, result.Detail);
}
