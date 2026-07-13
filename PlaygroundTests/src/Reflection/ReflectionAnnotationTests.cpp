#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Reflection;
import PlaygroundTests.ReflectionTestTypes;

using namespace ReflectionTestTypes;

TEST_CASE("field annotations expose their values")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Gadget>();

	const PgE::FieldInfo* health = type.FindFieldByIdentifier("Health");
	REQUIRE(health != nullptr);

	const auto ranges = health->GetAnnotations<Range>();
	REQUIRE(ranges.size() == 1);
	CHECK(ranges.front()->Min == 0.0);
	CHECK(ranges.front()->Max == 100.0);

	const auto docs = health->GetAnnotations<Doc>();
	REQUIRE(docs.size() == 1);
	CHECK(std::string_view{docs.front()->Text} == "hit points");

	CHECK(health->HasAnnotation<Range>());
	CHECK(health->GetAnnotations().size() == 2);

	// A cv/ref-qualified query normalizes to the same stored annotation.
	CHECK(health->GetAnnotations<const Range&>() == ranges);
	CHECK(health->HasAnnotation<const Range>());
}

TEST_CASE("repeated annotations of one type are all returned in declaration order")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Gadget>();

	const PgE::FieldInfo* nickname = type.FindFieldByIdentifier("Nickname");
	REQUIRE(nickname != nullptr);

	const auto docs = nickname->GetAnnotations<Doc>();
	REQUIRE(docs.size() == 2);
	CHECK(std::string_view{docs[0]->Text} == "primary");
	CHECK(std::string_view{docs[1]->Text} == "secondary");
}

TEST_CASE("absent annotations query as empty")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Gadget>();

	const PgE::FieldInfo* plain = type.FindFieldByIdentifier("Plain");
	REQUIRE(plain != nullptr);

	CHECK(plain->GetAnnotations<Range>().empty());
	CHECK_FALSE(plain->HasAnnotation<Doc>());
	CHECK(plain->GetAnnotations().empty());

	// An annotated field is still empty for a type it does not carry.
	const PgE::FieldInfo* health = type.FindFieldByIdentifier("Health");
	REQUIRE(health != nullptr);
	CHECK(health->GetAnnotations<Serializable>().empty());
}

TEST_CASE("annotations attach to types, functions, and parameters alike")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Gadget>();

	// Type-level annotation, through the same Annotated surface.
	CHECK(type.HasAnnotation<Serializable>());
	CHECK(type.GetAnnotations<Serializable>().size() == 1);

	const auto functions = type.FindFunctionsByIdentifier("Hurt");
	REQUIRE(functions.size() == 1);
	const PgE::FunctionInfo* hurt = functions.front();

	const auto functionDocs = hurt->GetAnnotations<Doc>();
	REQUIRE(functionDocs.size() == 1);
	CHECK(std::string_view{functionDocs.front()->Text} == "apply damage, return remaining health");

	// Parameter-level annotation.
	const std::span<const PgE::ParameterInfo> params = hurt->GetParams();
	REQUIRE(params.size() == 1);
	const auto paramRanges = params.front().GetAnnotations<Range>();
	REQUIRE(paramRanges.size() == 1);
	CHECK(paramRanges.front()->Max == 1000.0);
}

TEST_CASE("empty tag annotation is present but carries no data")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Gadget>();

	// The value pointer is non-null even for an empty tag: presence, not payload.
	const auto tags = type.GetAnnotations<Serializable>();
	REQUIRE(tags.size() == 1);
	CHECK(tags.front() != nullptr);
	CHECK(type.HasAnnotation<Serializable>());
}
