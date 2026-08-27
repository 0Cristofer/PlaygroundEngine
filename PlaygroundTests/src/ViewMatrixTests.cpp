#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Math;
import PlaygroundEngine.Renderer.View;

namespace
{
	constexpr float Tolerance = 1e-5f;

	void CheckVectorsClose(const PgE::Vector3 actual, const PgE::Vector3 expected)
	{
		CHECK(actual.X == doctest::Approx(expected.X).epsilon(Tolerance));
		CHECK(actual.Y == doctest::Approx(expected.Y).epsilon(Tolerance));
		CHECK(actual.Z == doctest::Approx(expected.Z).epsilon(Tolerance));
	}

	PgE::Vector3 TransformPoint(const PgE::Matrix4x4& matrix, const PgE::Vector3 point)
	{
		const PgE::Vector4 result = matrix * PgE::Vector4{.X = point.X, .Y = point.Y, .Z = point.Z, .W = 1.0f};
		return PgE::Vector3{.X = result.X, .Y = result.Y, .Z = result.Z};
	}

	PgE::Vector3 ToNormalizedDevice(const PgE::Matrix4x4& projection, const PgE::Vector3 viewPoint)
	{
		const PgE::Vector4 clip = projection * PgE::Vector4{.X = viewPoint.X, .Y = viewPoint.Y, .Z = viewPoint.Z, .W = 1.0f};
		return PgE::Vector3{.X = clip.X / clip.W, .Y = clip.Y / clip.W, .Z = clip.Z / clip.W};
	}

	constexpr float NearPlane = 0.1f;
	constexpr float FarPlane = 1000.0f;
}

TEST_CASE("The view matrix carries world forward onto -Z, world up onto +Y and world right onto +X")
{
	const PgE::Matrix4x4 worldToView = PgE::MakeWorldToViewMatrix(PgE::Vector3::Zero, PgE::Quaternion::Identity);

	CheckVectorsClose(TransformPoint(worldToView, PgE::Vector3::Forward), PgE::Vector3{.X = 0.0f, .Y = 0.0f, .Z = -1.0f});
	CheckVectorsClose(TransformPoint(worldToView, PgE::Vector3::Up), PgE::Vector3{.X = 0.0f, .Y = 1.0f, .Z = 0.0f});
	CheckVectorsClose(TransformPoint(worldToView, PgE::Vector3::Right), PgE::Vector3{.X = 1.0f, .Y = 0.0f, .Z = 0.0f});
}

TEST_CASE("The view matrix inverts the camera's own placement")
{
	const PgE::Vector3 position{.X = 2.0f, .Y = -2.0f, .Z = 2.0f};
	const PgE::Quaternion rotation =
		PgE::Quaternion::FromEulerAngles(PgE::EulerAngles{.Pitch = PgE::ToRadians(-35.264f), .Yaw = PgE::ToRadians(45.0f)});

	const PgE::Matrix4x4 worldToView = PgE::MakeWorldToViewMatrix(position, rotation);

	CheckVectorsClose(TransformPoint(worldToView, position), PgE::Vector3::Zero);

	const PgE::Vector3 ahead = position + rotation * PgE::Vector3::Forward;
	CheckVectorsClose(TransformPoint(worldToView, ahead), PgE::Vector3{.X = 0.0f, .Y = 0.0f, .Z = -1.0f});
}

TEST_CASE("The Z-up spelling of the demo's Y-up framing still looks at the origin")
{
	const PgE::Vector3 position{.X = 2.0f, .Y = -2.0f, .Z = 2.0f};
	const PgE::Quaternion rotation =
		PgE::Quaternion::FromEulerAngles(PgE::EulerAngles{.Pitch = PgE::ToRadians(-35.264f), .Yaw = PgE::ToRadians(45.0f)});

	CheckVectorsClose(rotation * PgE::Vector3::Forward, PgE::Vector3::Normalize(PgE::Vector3::Zero - position));
}

TEST_CASE("The projection puts the near plane at depth zero and the far plane at depth one")
{
	const PgE::Matrix4x4 projection = PgE::MakePerspectiveProjectionMatrix(PgE::ToRadians(45.0f), 1.0f, NearPlane, FarPlane);

	CHECK(ToNormalizedDevice(projection, PgE::Vector3{.Z = -NearPlane}).Z == doctest::Approx(0.0f).epsilon(Tolerance));
	CHECK(ToNormalizedDevice(projection, PgE::Vector3{.Z = -FarPlane}).Z == doctest::Approx(1.0f).epsilon(Tolerance));
}

TEST_CASE("The vertical field of view sets the frustum edges, which clip space places Y down")
{
	const float FieldOfView = PgE::ToRadians(45.0f);
	const PgE::Matrix4x4 projection = PgE::MakePerspectiveProjectionMatrix(FieldOfView, 1.0f, NearPlane, FarPlane);

	constexpr float Depth = 4.0f;
	const float halfHeight = Depth * std::tan(FieldOfView * 0.5f);

	CHECK(ToNormalizedDevice(projection, PgE::Vector3{.Y = halfHeight, .Z = -Depth}).Y == doctest::Approx(-1.0f).epsilon(Tolerance));
	CHECK(ToNormalizedDevice(projection, PgE::Vector3{.Y = -halfHeight, .Z = -Depth}).Y == doctest::Approx(1.0f).epsilon(Tolerance));
	CHECK(ToNormalizedDevice(projection, PgE::Vector3{.Z = -Depth}).Y == doctest::Approx(0.0f).epsilon(Tolerance));
}

TEST_CASE("The aspect ratio widens the frustum horizontally and leaves the vertical alone")
{
	const float FieldOfView = PgE::ToRadians(45.0f);
	constexpr float AspectRatio = 16.0f / 9.0f;

	const PgE::Matrix4x4 square = PgE::MakePerspectiveProjectionMatrix(FieldOfView, 1.0f, NearPlane, FarPlane);
	const PgE::Matrix4x4 wide = PgE::MakePerspectiveProjectionMatrix(FieldOfView, AspectRatio, NearPlane, FarPlane);

	const PgE::Vector3 offAxis{.X = 1.0f, .Y = 1.0f, .Z = -4.0f};

	CHECK(ToNormalizedDevice(wide, offAxis).X == doctest::Approx(ToNormalizedDevice(square, offAxis).X / AspectRatio).epsilon(Tolerance));
	CHECK(ToNormalizedDevice(wide, offAxis).Y == doctest::Approx(ToNormalizedDevice(square, offAxis).Y).epsilon(Tolerance));
}

TEST_CASE("A default extracted view carries a usable lens")
{
	const PgE::ExtractedView view;

	CHECK(view.VerticalFieldOfViewRadians == doctest::Approx(PgE::ToRadians(45.0f)).epsilon(Tolerance));
	CHECK(view.NearPlaneDistance == doctest::Approx(NearPlane));
	CHECK(view.FarPlaneDistance == doctest::Approx(FarPlane));
	CHECK(view.Position == PgE::Vector3::Zero);
	CHECK(view.Rotation == PgE::Quaternion::Identity);
}
