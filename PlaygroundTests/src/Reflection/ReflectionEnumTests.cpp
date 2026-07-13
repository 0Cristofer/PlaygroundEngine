#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Reflection;
import PlaygroundTests.ReflectionTestTypes;

using namespace ReflectionTestTypes;

TEST_CASE("enum field is accessible and stringifies to its enumerator name")
{
	Palette palette{};
	const PgE::TypeInfo& type = PgE::TypeOf<Palette>();

	CHECK(*type.GetFieldAs<Shade>(&palette, "Primary") == Shade::Green);
	REQUIRE(type.SetFieldAs(&palette, "Primary", Shade::Red).has_value());
	CHECK(palette.Primary == Shade::Red);

	// The enum name propagates through the owning struct's field walk.
	CHECK(PgE::ToString(palette) == "{Primary: Red, Count: 2}");
}

TEST_CASE("enumeration facet exposes enumerators, values, and underlying type")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Permissions>();
	REQUIRE(type.GetKind() == PgE::TypeKind::Enum);

	const PgE::EnumerationFacet* enumeration = type.GetFacet<PgE::EnumerationFacet>();
	REQUIRE(enumeration != nullptr);

	// The underlying type is reflected as the same TypeInfo instance TypeOf<uint16_t> yields.
	const PgE::TypeInfo& underlying = enumeration->GetUnderlyingType();
	CHECK(&underlying == &PgE::TypeOf<std::uint16_t>());
	CHECK(underlying.GetKind() == PgE::TypeKind::Integral);
	CHECK(underlying.GetSize() == sizeof(std::uint16_t));

	// Enumerators are listed in declaration order, each with its value.
	const auto enumerators = enumeration->GetEnumerators();
	REQUIRE(enumerators.size() == 4);
	CHECK(enumerators[0].GetIdentifier() == "None");
	CHECK(enumerators[0].GetValue() == 0);
	CHECK(enumerators[1].GetIdentifier() == "Read");
	CHECK(enumerators[1].GetValue() == 1);
	CHECK(enumerators[3].GetIdentifier() == "Execute");
	CHECK(enumerators[3].GetValue() == 4);

	// Lookups resolve both directions; a miss is a null result, not a crash.
	CHECK(enumeration->FindByIdentifier("Write")->GetValue() == 2);
	CHECK(enumeration->FindByValue(4)->GetIdentifier() == "Execute");
	CHECK(enumeration->FindByIdentifier("Delete") == nullptr);
	CHECK(enumeration->FindByValue(99) == nullptr);
}

TEST_CASE("non-enum types carry no enumeration facet")
{
	CHECK(PgE::TypeOf<int>().GetFacet<PgE::EnumerationFacet>() == nullptr);
	CHECK(PgE::TypeOf<Palette>().GetFacet<PgE::EnumerationFacet>() == nullptr);
}

TEST_CASE("enum name round-trips through the typed sugar")
{
	CHECK(PgE::EnumToName(Permissions::Write) == "Write");
	CHECK(PgE::EnumFromName<Permissions>("Execute") == Permissions::Execute);

	// A bit-or'd value no enumerator names has no exact name, but still renders numerically.
	constexpr auto combined =
		static_cast<Permissions>(static_cast<std::uint16_t>(Permissions::Read) | static_cast<std::uint16_t>(Permissions::Write));
	CHECK(PgE::EnumToName(combined) == std::nullopt);
	CHECK(PgE::ToString(combined) == "3");

	// An unknown name does not parse.
	CHECK(PgE::EnumFromName<Permissions>("Sudo") == std::nullopt);
}

TEST_CASE("signed enumerator round-trips through the uint64 bit pattern")
{
	const PgE::EnumerationFacet* enumeration = PgE::TypeOf<Temperature>().GetFacet<PgE::EnumerationFacet>();
	REQUIRE(enumeration != nullptr);
	CHECK(&enumeration->GetUnderlyingType() == &PgE::TypeOf<std::int16_t>());

	// -10 as int16 widened to uint64 is its two's-complement bit pattern; the stored value matches, and a
	// lookup by that pattern recovers the name.
	constexpr auto freezingBits = static_cast<std::uint64_t>(static_cast<std::int16_t>(-10));
	CHECK(enumeration->FindByIdentifier("Freezing")->GetValue() == freezingBits);
	CHECK(enumeration->FindByValue(freezingBits)->GetIdentifier() == "Freezing");

	CHECK(PgE::EnumToName(Temperature::Freezing) == "Freezing");
	CHECK(PgE::EnumFromName<Temperature>("Freezing") == Temperature::Freezing);
	CHECK(PgE::ToString(Temperature::Freezing) == "Freezing");

	// An unnamed negative value falls back to its signed number, not the raw uint64 bits.
	CHECK(PgE::ToString(static_cast<Temperature>(-5)) == "-5");
}

TEST_CASE("unscoped enum is reflected like a scoped one")
{
	const PgE::EnumerationFacet* enumeration = PgE::TypeOf<Direction>().GetFacet<PgE::EnumerationFacet>();
	REQUIRE(enumeration != nullptr);
	CHECK(enumeration->GetEnumerators().size() == 4);
	CHECK(PgE::EnumToName(Direction::East) == "East");
	CHECK(PgE::ToString(Direction::West) == "West");
}
