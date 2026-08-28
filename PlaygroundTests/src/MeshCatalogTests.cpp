#include <doctest/doctest.h>

import std;
import PlaygroundEngine.MeshCatalog;
import PlaygroundEngine.Renderer.Mesh;

TEST_CASE("A path that was never inserted resolves to nothing")
{
	const PgE::MeshCatalog catalog;

	CHECK_FALSE(catalog.Contains("cube.obj"));
	CHECK(catalog.Find("cube.obj").Index == PgE::MeshHandle::InvalidIndex);
}

TEST_CASE("An inserted path resolves to the handle it was given")
{
	PgE::MeshCatalog catalog;
	catalog.Insert("cube.obj", PgE::MeshHandle{.Index = 7});

	CHECK(catalog.Contains("cube.obj"));
	CHECK(catalog.Find("cube.obj") == PgE::MeshHandle{.Index = 7});
}

// The distinction the frame loop relies on to stop retrying a load that already failed.
TEST_CASE("A remembered failure is told apart from a path never tried")
{
	PgE::MeshCatalog catalog;
	catalog.Insert("broken.obj", PgE::MeshHandle{});

	CHECK(catalog.Contains("broken.obj"));
	CHECK(catalog.Find("broken.obj").Index == PgE::MeshHandle::InvalidIndex);
	CHECK_FALSE(catalog.Contains("untried.obj"));
}

TEST_CASE("Inserting a path again replaces the handle it resolved to")
{
	PgE::MeshCatalog catalog;
	catalog.Insert("cube.obj", PgE::MeshHandle{});
	catalog.Insert("cube.obj", PgE::MeshHandle{.Index = 2});

	CHECK(catalog.Find("cube.obj") == PgE::MeshHandle{.Index = 2});
}
