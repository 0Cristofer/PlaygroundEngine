#include <cstddef>
#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Reflection;
import PlaygroundTests.SnapshotHarness;

// A named namespace so reflected identifiers and display strings are deterministic across runs.
namespace HarnessSelfCheckTypes
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

	struct FieldsInOrder
	{
		char First;
		int Second;
	};

	struct FieldsReordered
	{
		int Second;
		char First;
	};

	struct FieldsExtended
	{
		char First;
		int Second;
		int Third;
	};
}

using namespace PgE;
using HarnessSelfCheckTypes::FieldsExtended;
using HarnessSelfCheckTypes::FieldsInOrder;
using HarnessSelfCheckTypes::FieldsReordered;
using HarnessSelfCheckTypes::Packed;
using HarnessSelfCheckTypes::Padded;

// Ground-truth cross-check: the describer sources its layout numbers from the reflection system, so
// anchor them to the language's own operators (computed by a different path) so a wrong trait cannot
// be blessed into a golden as truth. See docs/CorrectnessAndStandards.md.
template <typename T>
static void CheckReflectedTraitsMatchLanguage()
{
	const TypeTraits& traits = TypeOf<T>().GetTraits();

	CHECK(traits.Size == sizeof(T));
	CHECK(traits.Alignment == alignof(T));
	CHECK(traits.IsTriviallyCopyable == std::is_trivially_copyable_v<T>);
	CHECK(traits.IsStandardLayout == std::is_standard_layout_v<T>);
	CHECK(traits.HasUniqueObjectRepresentations == std::has_unique_object_representations_v<T>);
}

TEST_CASE("reflected traits match the language's own type operators")
{
	CheckReflectedTraitsMatchLanguage<Packed>();
	CheckReflectedTraitsMatchLanguage<Padded>();
	CheckReflectedTraitsMatchLanguage<int>();
	CheckReflectedTraitsMatchLanguage<double>();
}

TEST_CASE("reflected field offsets match offsetof")
{
	const std::span<const FieldInfo> fields = TypeOf<Padded>().GetFields();
	REQUIRE(fields.size() == 2);

	CHECK(static_cast<std::size_t>(fields[0].GetByteOffset()) == offsetof(Padded, C));
	CHECK(static_cast<std::size_t>(fields[1].GetByteOffset()) == offsetof(Padded, I));
}

namespace
{
	// The description minus its leading `type <name>` line, so a discrimination check tests the
	// structural body (kinds, field order, offsets) rather than passing trivially on the type name.
	std::string_view DescribedBody(std::string_view described)
	{
		const std::size_t firstNewline = described.find('\n');
		return firstNewline == std::string_view::npos ? described : described.substr(firstNewline + 1);
	}
}

// Discrimination. A snapshot only guards what its describer is sensitive to, so prove the describer
// actually reflects a reorder and an added field rather than assuming it does.
TEST_CASE("DescribeType body changes when fields are reordered")
{
	const std::string ordered = Snapshot::DescribeType<FieldsInOrder>();
	const std::string reordered = Snapshot::DescribeType<FieldsReordered>();

	CHECK(DescribedBody(ordered) != DescribedBody(reordered));
}

TEST_CASE("DescribeType body changes when a field is added")
{
	const std::string base = Snapshot::DescribeType<FieldsInOrder>();
	const std::string extended = Snapshot::DescribeType<FieldsExtended>();

	CHECK(DescribedBody(base) != DescribedBody(extended));
}
