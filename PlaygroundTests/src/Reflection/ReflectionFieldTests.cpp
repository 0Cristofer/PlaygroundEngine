#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Reflection;
import PlaygroundTests.ReflectionTestTypes;

using namespace ReflectionTestTypes;

TEST_CASE("typed field access reads and writes by name")
{
	Person person{"John", 25};
	const PgE::TypeInfo& type = PgE::TypeOf<Person>();

	const auto age = type.GetFieldAs<int>(&person, "Age");
	REQUIRE(age.has_value());
	CHECK(*age == 25);

	REQUIRE(type.SetFieldAs(&person, "Age", 30).has_value());
	CHECK(person.Age == 30);

	const auto name = type.GetFieldAs<std::string>(&person, "Name");
	REQUIRE(name.has_value());
	CHECK(*name == "John");
}

TEST_CASE("bitfields read and write like any field")
{
	Packet packet{};
	const PgE::TypeInfo& type = PgE::TypeOf<Packet>();

	CHECK(*type.GetFieldAs<unsigned>(&packet, "Version") == 3u);
	CHECK(*type.GetFieldAs<unsigned>(&packet, "Flags") == 10u);

	REQUIRE(type.SetFieldAs(&packet, "Version", 9u).has_value());
	CHECK(packet.Version == 9u);
	CHECK(packet.Flags == 10u);
	CHECK(packet.Payload == 100);
}

TEST_CASE("private fields are readable and writable through reflection")
{
	Secret secret{};
	const PgE::TypeInfo& type = PgE::TypeOf<Secret>();

	const auto hidden = type.GetFieldAs<int>(&secret, "_hidden");
	REQUIRE(hidden.has_value());
	CHECK(*hidden == 7);

	REQUIRE(type.SetFieldAs(&secret, "_hidden", 42).has_value());
	CHECK(secret.Reveal() == 42);
}

TEST_CASE("erased field access moves values through a typed slot")
{
	Person person{.Name = "Jane", .Age = 40};
	const PgE::TypeInfo& type = PgE::TypeOf<Person>();

	int readBack = 0;
	const auto got = type.GetFieldValue(&person, "Age", {.Type = &PgE::TypeOf<int>(), .Data = &readBack, .IsConst = false});
	REQUIRE(got.has_value());
	CHECK(readBack == 40);

	int newAge = 41;
	const auto set = type.SetFieldValue(&person, "Age", {.Type = &PgE::TypeOf<int>(), .Data = &newAge, .IsConst = true});
	REQUIRE(set.has_value());
	CHECK(person.Age == 41);
}

TEST_CASE("field access reports missing names and type mismatches")
{
	Person person{.Name = "Jane", .Age = 40};
	const PgE::TypeInfo& type = PgE::TypeOf<Person>();

	const auto missing = type.GetFieldAs<int>(&person, "Nope");
	REQUIRE_FALSE(missing.has_value());
	CHECK(missing.error().Reason == PgE::FieldError::FieldNotFound);

	const auto mistyped = type.GetFieldAs<std::string>(&person, "Age");
	REQUIRE_FALSE(mistyped.has_value());
	CHECK(mistyped.error().Reason == PgE::FieldError::TypeMismatch);
}

TEST_CASE("const member is readable but not writable")
{
	Fixed fixed{};
	const PgE::TypeInfo& type = PgE::TypeOf<Fixed>();

	CHECK(*type.GetFieldAs<int>(&fixed, "Constant") == 5);

	const auto rejected = type.SetFieldAs(&fixed, "Constant", 9);
	REQUIRE_FALSE(rejected.has_value());
	CHECK(rejected.error().Reason == PgE::FieldError::NotWritable);
	CHECK(fixed.Constant == 5);

	REQUIRE(type.SetFieldAs(&fixed, "Variable", 9).has_value());
	CHECK(fixed.Variable == 9);
}

TEST_CASE("field access through a resolved FieldInfo")
{
	Person person{.Name = "John", .Age = 25};

	const PgE::FieldInfo* age = PgE::TypeOf<Person>().FindFieldByIdentifier("Age");
	REQUIRE(age != nullptr);
	CHECK(age->GetIdentifier() == "Age");
	CHECK(&age->GetTypeInfo() == &PgE::TypeOf<int>());

	const auto typed = age->GetAs<int>(&person);
	REQUIRE(typed.has_value());
	CHECK(*typed == 25);

	REQUIRE(age->SetAs(&person, 30).has_value());
	CHECK(person.Age == 30);

	int readBack = 0;
	REQUIRE(age->GetValue(&person, {&PgE::TypeOf<int>(), &readBack, false}).has_value());
	CHECK(readBack == 30);

	int newAge = 31;
	REQUIRE(age->SetValue(&person, {&PgE::TypeOf<int>(), &newAge, true}).has_value());
	CHECK(person.Age == 31);
}

TEST_CASE("FindFieldByIdentifier misses return nullptr")
{
	CHECK(PgE::TypeOf<Person>().FindFieldByIdentifier("Nope") == nullptr);
}

TEST_CASE("reference members read through to the referent and are not writable")
{
	Referencing referencing{};
	const PgE::TypeInfo& type = PgE::TypeOf<Referencing>();

	const PgE::FieldInfo* alias = type.FindFieldByIdentifier("Alias");
	REQUIRE(alias != nullptr);
	CHECK(&alias->GetTypeInfo() == &PgE::TypeOf<int>());

	CHECK(*type.GetFieldAs<int>(&referencing, "Alias") == 100);

	// Reading dereferences the live reference: mutating the referent shows through on the next read.
	referencing.Target = 55;
	CHECK(*type.GetFieldAs<int>(&referencing, "Alias") == 55);
	CHECK(*type.GetFieldAs<int>(&referencing, "ConstAlias") == 55);

	const auto rebindMutable = type.SetFieldAs(&referencing, "Alias", 7);
	REQUIRE_FALSE(rebindMutable.has_value());
	CHECK(rebindMutable.error().Reason == PgE::FieldError::NotWritable);

	const auto rebindConst = type.SetFieldAs(&referencing, "ConstAlias", 7);
	REQUIRE_FALSE(rebindConst.has_value());
	CHECK(rebindConst.error().Reason == PgE::FieldError::NotWritable);

	CHECK(referencing.Target == 55);
}

TEST_CASE("borrowed field reference reads and writes in place")
{
	Person person{"John", 25};
	const PgE::TypeInfo& type = PgE::TypeOf<Person>();

	const auto ref = type.GetFieldRef(&person, "Age");
	REQUIRE(ref.has_value());
	CHECK(ref->Data == &person.Age);
	CHECK_FALSE(ref->IsConst);

	auto age = type.GetFieldRefAs<int>(&person, "Age");
	REQUIRE(age.has_value());
	age->get() = 99;
	CHECK(person.Age == 99);
}

TEST_CASE("bitfields have no borrow")
{
	Packet packet{};
	const auto ref = PgE::TypeOf<Packet>().GetFieldRef(&packet, "Version");
	REQUIRE_FALSE(ref.has_value());
	CHECK(ref.error().Reason == PgE::FieldError::NotAddressable);
}

TEST_CASE("borrow constness follows the object and the member")
{
	Fixed fixed{};
	const PgE::TypeInfo& type = PgE::TypeOf<Fixed>();

	const auto viaConstObject = type.GetFieldRef(&std::as_const(fixed), "Variable");
	REQUIRE(viaConstObject.has_value());
	CHECK(viaConstObject->IsConst);

	const auto mutableConstMember = type.GetFieldRefAs<int>(&fixed, "Constant");
	REQUIRE_FALSE(mutableConstMember.has_value());
	CHECK(mutableConstMember.error().Reason == PgE::FieldError::NotWritable);

	const Fixed& readOnly = fixed;
	const auto readConstMember = type.GetFieldRefAs<int>(&readOnly, "Constant");
	REQUIRE(readConstMember.has_value());
	CHECK(readConstMember->get() == 5);
}

TEST_CASE("borrowed reference member assigns through to the referent")
{
	Referencing referencing{};
	const PgE::TypeInfo& type = PgE::TypeOf<Referencing>();

	const auto aliasRef = type.GetFieldRef(&referencing, "Alias");
	REQUIRE(aliasRef.has_value());
	CHECK(aliasRef->Data == &referencing.Target);
	CHECK_FALSE(aliasRef->IsConst);

	auto alias = type.GetFieldRefAs<int>(&referencing, "Alias");
	REQUIRE(alias.has_value());
	alias->get() = 77;
	CHECK(referencing.Target == 77);

	const auto constAlias = type.GetFieldRef(&referencing, "ConstAlias");
	REQUIRE(constAlias.has_value());
	CHECK(constAlias->IsConst);
}

TEST_CASE("move-only field is settable by move, not by copy")
{
	MoveOnlyHolder holder{};
	const PgE::TypeInfo& type = PgE::TypeOf<MoveOnlyHolder>();

	const auto valueGet = type.GetFieldAs<MoveOnlyValue>(&holder, "Item");
	REQUIRE_FALSE(valueGet.has_value());
	CHECK(valueGet.error().Reason == PgE::FieldError::NotReadable);

	MoveOnlyValue source;
	source.Tag = 9;
	const auto copySet = type.SetFieldAs(&holder, "Item", source);
	REQUIRE_FALSE(copySet.has_value());
	CHECK(copySet.error().Reason == PgE::FieldError::NotWritable);

	const auto moveSet = type.MoveFieldAs(&holder, "Item", source);
	REQUIRE(moveSet.has_value());
	CHECK(holder.Item.Tag == 9);

	auto borrow = type.GetFieldRefAs<MoveOnlyValue>(&holder, "Item");
	REQUIRE(borrow.has_value());
	CHECK(borrow->get().Tag == 9);
}

TEST_CASE("non-copyable non-movable field is reachable only through the borrow")
{
	ImmovableHolder holder{};
	const PgE::TypeInfo& type = PgE::TypeOf<ImmovableHolder>();

	const auto get = type.GetFieldValue(&holder, "Item", {&PgE::TypeOf<Immovable>(), nullptr, false});
	REQUIRE_FALSE(get.has_value());
	CHECK(get.error().Reason == PgE::FieldError::NotReadable);

	Immovable other;
	const auto move = type.MoveFieldAs(&holder, "Item", other);
	REQUIRE_FALSE(move.has_value());
	CHECK(move.error().Reason == PgE::FieldError::NotWritable);

	auto borrow = type.GetFieldRefAs<Immovable>(&holder, "Item");
	REQUIRE(borrow.has_value());
	borrow->get().Tag = 5;
	CHECK(holder.Item.Tag == 5);
}

// A nested reflected struct: value get/set copies the whole sub-object, the borrow mutates it in
// place, and stringification recurses. A pointer member reads and writes as a value.
TEST_CASE("nested struct and pointer fields")
{
	Outer outer{};
	const PgE::TypeInfo& type = PgE::TypeOf<Outer>();

	CHECK(type.GetFieldAs<Inner>(&outer, "Child")->A == 1);
	type.GetFieldRefAs<Inner>(&outer, "Child")->get().A = 99;
	CHECK(outer.Child.A == 99);

	int target = 0;
	REQUIRE(type.SetFieldAs(&outer, "Pointer", &target).has_value());
	CHECK(outer.Pointer == &target);
	CHECK(*type.GetFieldAs<int*>(&outer, "Pointer") == &target);

	outer.Child.A = 1;
	outer.Pointer = nullptr;
	CHECK(PgE::ToString(outer).starts_with("{Child: {A: 1}, Pointer: "));
	CHECK(PgE::ToString(outer).ends_with(", Plain: 7}"));
}

TEST_CASE("union members are reflected and accessible")
{
	Scalar scalar{};
	const PgE::TypeInfo& type = PgE::TypeOf<Scalar>();

	const PgE::FieldInfo* asInt = type.FindFieldByIdentifier("AsInt");
	REQUIRE(asInt != nullptr);
	CHECK(&asInt->GetTypeInfo() == &PgE::TypeOf<int>());

	// Each field reads the member that was last written (the active one), so access is well-defined.
	REQUIRE(type.SetFieldAs(&scalar, "AsInt", 7).has_value());
	CHECK(*type.GetFieldAs<int>(&scalar, "AsInt") == 7);

	REQUIRE(type.SetFieldAs(&scalar, "AsFloat", 1.5f).has_value());
	CHECK(*type.GetFieldAs<float>(&scalar, "AsFloat") == 1.5f);
}
