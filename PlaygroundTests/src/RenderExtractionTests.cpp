#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Ecs;
import PlaygroundEngine.Ecs.CameraComponent;
import PlaygroundEngine.Math;
import PlaygroundEngine.RenderExtraction;
import PlaygroundEngine.Renderer.Frame;
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

	PgE::ExtractedFrame ExtractFrom(const PgE::Ecs& ecs)
	{
		PgE::ExtractedFrame frame;
		PgE::ExtractFrame(ecs, frame);

		return frame;
	}
}

TEST_CASE("A camera and its transform extract into the frame's view, its lens converted to radians")
{
	PgE::Ecs ecs;

	const PgE::Vector3 position{.X = 1.0f, .Y = 2.0f, .Z = 3.0f};
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

	const PgE::Vector3 first{.X = 1.0f};
	const PgE::Vector3 second{.X = 2.0f};

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

	PgE::ExtractFrame(ecs, frame);

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
