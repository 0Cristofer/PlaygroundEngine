#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Ecs;
import PlaygroundEngine.Ecs.CameraComponent;
import PlaygroundEngine.Ecs.MeshComponent;
import PlaygroundEngine.Math;
import PlaygroundEngine.MeshCatalog;
import PlaygroundEngine.RenderExtraction;
import PlaygroundEngine.Renderer.Frame;
import PlaygroundEngine.Renderer.Mesh;
import PlaygroundEngine.Renderer.View;

namespace
{
	PgE::Entity SpawnCamera(PgE::Ecs& ecs, const PgE::Vector3 position)
	{
		const PgE::Entity camera = ecs.AddEntity();

		ecs.AddComponentToEntity<PgE::TransformComponent>(camera)->Position = position;
		ecs.AddComponentToEntity<PgE::CameraComponent>(camera);

		return camera;
	}

	PgE::ExtractedFrame ExtractFrom(const PgE::Ecs& ecs, const PgE::MeshCatalog& meshes = {})
	{
		PgE::ExtractedFrame frame;
		PgE::ExtractFrame(ecs, meshes, frame);

		return frame;
	}

	PgE::Entity SpawnMesh(PgE::Ecs& ecs, const std::string& path, const PgE::Vector3 position)
	{
		const PgE::Entity entity = ecs.AddEntity();

		ecs.AddComponentToEntity<PgE::TransformComponent>(entity)->Position = position;
		ecs.AddComponentToEntity<PgE::MeshComponent>(entity)->MeshPath = path;

		return entity;
	}

	PgE::MeshCatalog CatalogOf(const std::string& path, const std::uint32_t index)
	{
		PgE::MeshCatalog catalog;
		catalog.Insert(path, PgE::MeshHandle{.Index = index});

		return catalog;
	}
}

TEST_CASE("A camera and its transform extract into the frame's view, its lens converted to radians")
{
	PgE::Ecs ecs;

	constexpr PgE::Vector3 position{.X = 1.0f, .Y = 2.0f, .Z = 3.0f};
	const PgE::Quaternion rotation = PgE::Quaternion::FromAxisAngle(PgE::Vector3::Up, PgE::ToRadians(90.0f));

	const PgE::Entity camera = SpawnCamera(ecs, position);
	ecs.TryGetComponent<PgE::TransformComponent>(camera)->Rotation = rotation;
	ecs.TryGetComponent<PgE::CameraComponent>(camera)->VerticalFieldOfViewDegrees = 60.0f;

	const PgE::ExtractedFrame frame = ExtractFrom(ecs);

	CHECK(frame.View.Position == position);
	CHECK(frame.View.Rotation == rotation);
	CHECK(frame.View.VerticalFieldOfViewRadians == doctest::Approx(PgE::ToRadians(60.0f)));
}

TEST_CASE("A world with no camera is drawn through the default view")
{
	PgE::Ecs ecs;
	ecs.AddComponentToEntity<PgE::TransformComponent>(ecs.AddEntity());

	const PgE::ExtractedFrame frame = ExtractFrom(ecs);

	CHECK(frame.View.Position == PgE::ExtractedView{}.Position);
	CHECK(frame.View.Rotation == PgE::ExtractedView{}.Rotation);
}

TEST_CASE("A camera with nowhere to look from is skipped")
{
	PgE::Ecs ecs;
	ecs.AddComponentToEntity<PgE::CameraComponent>(ecs.AddEntity());

	CHECK(ExtractFrom(ecs).View.Position == PgE::ExtractedView{}.Position);
}

TEST_CASE("The lowest entity id decides between cameras")
{
	PgE::Ecs ecs;

	constexpr PgE::Vector3 first{.X = 1.0f};
	constexpr PgE::Vector3 second{.X = 2.0f};

	SpawnCamera(ecs, first);
	SpawnCamera(ecs, second);

	CHECK(ExtractFrom(ecs).View.Position == first);
}

TEST_CASE("Extraction overwrites what the previous frame left in the storage")
{
	PgE::Ecs ecs;
	SpawnCamera(ecs, PgE::Vector3{.X = 7.0f});

	PgE::ExtractedFrame frame;
	frame.View.Position = PgE::Vector3{.X = -1.0f, .Y = -1.0f, .Z = -1.0f};

	PgE::ExtractFrame(ecs, PgE::MeshCatalog{}, frame);

	CHECK(frame.View.Position == PgE::Vector3{.X = 7.0f});
}

TEST_CASE("A lens dragged through zero still extracts a usable projection")
{
	PgE::Ecs ecs;
	const PgE::Entity camera = SpawnCamera(ecs, PgE::Vector3::Zero);

	const std::shared_ptr<PgE::CameraComponent> lens = ecs.TryGetComponent<PgE::CameraComponent>(camera);

	SUBCASE("a zero field of view")
	{
		lens->VerticalFieldOfViewDegrees = 0.0f;
	}
	SUBCASE("a negative field of view")
	{
		lens->VerticalFieldOfViewDegrees = -30.0f;
	}
	SUBCASE("a zero near plane")
	{
		lens->NearPlaneDistance = 0.0f;
	}
	SUBCASE("a far plane behind the near plane")
	{
		lens->NearPlaneDistance = 10.0f;
		lens->FarPlaneDistance = 10.0f;
	}

	const PgE::ExtractedView view = ExtractFrom(ecs).View;

	CHECK(view.VerticalFieldOfViewRadians > 0.0f);
	CHECK(view.VerticalFieldOfViewRadians < std::numbers::pi_v<float>);
	CHECK(view.NearPlaneDistance > 0.0f);
	CHECK(view.FarPlaneDistance > view.NearPlaneDistance);

	const PgE::Matrix4x4 projection =
		PgE::MakePerspectiveProjectionMatrix(view.VerticalFieldOfViewRadians, 1.0f, view.NearPlaneDistance, view.FarPlaneDistance);

	for (const PgE::Vector4& column : projection.Columns)
	{
		CHECK(std::isfinite(column.X));
		CHECK(std::isfinite(column.Y));
		CHECK(std::isfinite(column.Z));
		CHECK(std::isfinite(column.W));
	}
}

TEST_CASE("A mesh the catalog resolved extracts with the placement its entity carries")
{
	PgE::Ecs ecs;

	constexpr PgE::Vector3 position{.X = 4.0f, .Y = 5.0f, .Z = 6.0f};
	const PgE::Entity entity = SpawnMesh(ecs, "cube.obj", position);
	ecs.TryGetComponent<PgE::TransformComponent>(entity)->Scale = PgE::Vector3{.X = 2.0f, .Y = 2.0f, .Z = 2.0f};

	const PgE::ExtractedFrame frame = ExtractFrom(ecs, CatalogOf("cube.obj", 3));

	REQUIRE(frame.Meshes.size() == 1);
	CHECK(frame.Meshes.front().Mesh == PgE::MeshHandle{.Index = 3});
	CHECK(frame.Meshes.front().Placement.Position == position);
	CHECK(frame.Meshes.front().Placement.Scale == PgE::Vector3{.X = 2.0f, .Y = 2.0f, .Z = 2.0f});
}

TEST_CASE("A mesh path the catalog never resolved draws nothing")
{
	PgE::Ecs ecs;
	SpawnMesh(ecs, "missing.obj", PgE::Vector3::Zero);

	CHECK(ExtractFrom(ecs, CatalogOf("cube.obj", 1)).Meshes.empty());
}

TEST_CASE("A mesh with nowhere to stand is skipped")
{
	PgE::Ecs ecs;
	ecs.AddComponentToEntity<PgE::MeshComponent>(ecs.AddEntity())->MeshPath = "cube.obj";

	CHECK(ExtractFrom(ecs, CatalogOf("cube.obj", 1)).Meshes.empty());
}

TEST_CASE("Meshes extract in entity id order")
{
	PgE::Ecs ecs;

	SpawnMesh(ecs, "cube.obj", PgE::Vector3{.X = 1.0f});
	SpawnMesh(ecs, "cube.obj", PgE::Vector3{.X = 2.0f});
	SpawnMesh(ecs, "cube.obj", PgE::Vector3{.X = 3.0f});

	const PgE::ExtractedFrame frame = ExtractFrom(ecs, CatalogOf("cube.obj", 1));

	REQUIRE(frame.Meshes.size() == 3);
	CHECK(frame.Meshes[0].Placement.Position.X == 1.0f);
	CHECK(frame.Meshes[1].Placement.Position.X == 2.0f);
	CHECK(frame.Meshes[2].Placement.Position.X == 3.0f);
}

TEST_CASE("Re-extracting into the same frame replaces the draw list rather than appending to it")
{
	PgE::Ecs ecs;
	SpawnMesh(ecs, "cube.obj", PgE::Vector3::Zero);

	const PgE::MeshCatalog catalog = CatalogOf("cube.obj", 1);

	PgE::ExtractedFrame frame;
	PgE::ExtractFrame(ecs, catalog, frame);
	PgE::ExtractFrame(ecs, catalog, frame);

	CHECK(frame.Meshes.size() == 1);
}
