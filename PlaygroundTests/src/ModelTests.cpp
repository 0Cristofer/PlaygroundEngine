#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Model;

namespace
{
	// A unit quad as two triangles sharing an edge, with distinct positions, one normal and four
	// texture coordinates. Written inline so the tests exercise parsing alone, and shaped so the two
	// shared corners pin the deduplication.

	constexpr std::string_view QuadObject = R"(
v -1.0 -1.0 0.0
v  1.0 -1.0 0.0
v  1.0  1.0 0.0
v -1.0  1.0 0.0

vt 0.0 0.0
vt 1.0 0.0
vt 1.0 1.0
vt 0.0 1.0

vn 0.0 0.0 1.0

f 1/1/1 2/2/1 3/3/1
f 1/1/1 3/3/1 4/4/1
)";

	constexpr std::string_view PositionsOnlyTriangle = R"(
v 0.0 0.0 0.0
v 1.0 0.0 0.0
v 0.0 1.0 0.0

f 1 2 3
)";
}

TEST_CASE("ParseWavefrontMesh merges vertices shared between faces")
{
	const std::expected<PgE::Mesh, PgE::ModelError> result = PgE::ParseWavefrontMesh(QuadObject);

	REQUIRE(result.has_value());

	// Six index entries across the two triangles, but only four distinct corners: the two the
	// triangles share resolve to indices already emitted.

	CHECK(result->Indices.size() == 6);
	CHECK(result->Vertices.size() == 4);

	const std::vector<std::uint32_t> expectedIndices = {0, 1, 2, 0, 2, 3};
	CHECK(result->Indices == expectedIndices);
}

TEST_CASE("ParseWavefrontMesh flips the Wavefront V axis to a top-left origin")
{
	const std::expected<PgE::Mesh, PgE::ModelError> result = PgE::ParseWavefrontMesh(QuadObject);

	REQUIRE(result.has_value());
	REQUIRE(result->Vertices.size() == 4);

	// The file's first corner is at v = 0.0, which is the bottom of the image in Wavefront's
	// convention and therefore v = 1.0 once the origin moves to the top left.

	CHECK(result->Vertices[0].TextureCoordinate.x == doctest::Approx(0.0f));
	CHECK(result->Vertices[0].TextureCoordinate.y == doctest::Approx(1.0f));

	CHECK(result->Vertices[2].TextureCoordinate.x == doctest::Approx(1.0f));
	CHECK(result->Vertices[2].TextureCoordinate.y == doctest::Approx(0.0f));
}

TEST_CASE("ParseWavefrontMesh reads positions and normals into the mesh")
{
	const std::expected<PgE::Mesh, PgE::ModelError> result = PgE::ParseWavefrontMesh(QuadObject);

	REQUIRE(result.has_value());
	REQUIRE(result->Vertices.size() == 4);

	CHECK(result->Vertices[0].Position.x == doctest::Approx(-1.0f));
	CHECK(result->Vertices[0].Position.y == doctest::Approx(-1.0f));
	CHECK(result->Vertices[0].Position.z == doctest::Approx(0.0f));

	CHECK(result->Vertices[2].Position.x == doctest::Approx(1.0f));
	CHECK(result->Vertices[2].Position.y == doctest::Approx(1.0f));

	CHECK(result->Vertices[1].Normal.z == doctest::Approx(1.0f));
}

TEST_CASE("ParseWavefrontMesh defaults the attributes a face omits")
{
	// Faces may reference a position alone, leaving the normal and texture coordinate indices
	// negative. Those have to read as zero rather than index the attribute arrays from the end.

	const std::expected<PgE::Mesh, PgE::ModelError> result = PgE::ParseWavefrontMesh(PositionsOnlyTriangle);

	REQUIRE(result.has_value());
	REQUIRE(result->Vertices.size() == 3);

	CHECK(result->Vertices[1].Position.x == doctest::Approx(1.0f));

	CHECK(result->Vertices[1].Normal.x == doctest::Approx(0.0f));
	CHECK(result->Vertices[1].Normal.y == doctest::Approx(0.0f));
	CHECK(result->Vertices[1].Normal.z == doctest::Approx(0.0f));

	CHECK(result->Vertices[1].TextureCoordinate.x == doctest::Approx(0.0f));
	CHECK(result->Vertices[1].TextureCoordinate.y == doctest::Approx(0.0f));
}

TEST_CASE("ParseWavefrontMesh reports a failure for text holding no faces")
{
	const std::expected<PgE::Mesh, PgE::ModelError> result = PgE::ParseWavefrontMesh("v 0.0 0.0 0.0\n");

	REQUIRE_FALSE(result.has_value());
	CHECK(result.error() == PgE::ModelError::NoGeometry);
}

TEST_CASE("ParseWavefrontMesh reports a failure for an empty document")
{
	const std::expected<PgE::Mesh, PgE::ModelError> result = PgE::ParseWavefrontMesh("");

	REQUIRE_FALSE(result.has_value());
	CHECK(result.error() == PgE::ModelError::NoGeometry);
}
