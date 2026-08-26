#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Math;
import PlaygroundEngine.Reflection;
import PlaygroundTests.ContractSeam;

namespace
{
	constexpr float Tolerance = 1e-5f;

	void CheckVectorsClose(const PgE::Vector3 actual, const PgE::Vector3 expected)
	{
		CHECK(actual.X == doctest::Approx(expected.X).epsilon(Tolerance));
		CHECK(actual.Y == doctest::Approx(expected.Y).epsilon(Tolerance));
		CHECK(actual.Z == doctest::Approx(expected.Z).epsilon(Tolerance));
	}

	constexpr float QuarterTurn = std::numbers::pi_v<float> / 2.0f;
}

TEST_CASE("The world basis is right-handed with Z up")
{
	CHECK(PgE::Vector3::Right == PgE::Vector3{.X = 1.0f, .Y = 0.0f, .Z = 0.0f});
	CHECK(PgE::Vector3::Forward == PgE::Vector3{.X = 0.0f, .Y = 1.0f, .Z = 0.0f});
	CHECK(PgE::Vector3::Up == PgE::Vector3{.X = 0.0f, .Y = 0.0f, .Z = 1.0f});

	// Handedness is exactly cross(X, Y) == Z; a left-handed basis would yield -Up here.
	CheckVectorsClose(PgE::Vector3::Cross(PgE::Vector3::Right, PgE::Vector3::Forward), PgE::Vector3::Up);
}

TEST_CASE("Positive rotation carries each axis into the next")
{
	// The right-hand rule, stated as the engine relies on it: about X, +Y goes to +Z; about Y, +Z to +X;
	// about Z, +X to +Y. Getting any sign wrong here inverts a control scheme somewhere.

	SUBCASE("pitch about X is nose up")
	{
		const PgE::Quaternion pitch = PgE::Quaternion::FromAxisAngle(PgE::Vector3::Right, QuarterTurn);
		CheckVectorsClose(pitch * PgE::Vector3::Forward, PgE::Vector3::Up);
	}

	SUBCASE("roll about Y banks right")
	{
		const PgE::Quaternion roll = PgE::Quaternion::FromAxisAngle(PgE::Vector3::Forward, QuarterTurn);
		CheckVectorsClose(roll * PgE::Vector3::Up, PgE::Vector3::Right);
	}

	SUBCASE("yaw about Z turns left when facing forward")
	{
		const PgE::Quaternion yaw = PgE::Quaternion::FromAxisAngle(PgE::Vector3::Up, QuarterTurn);
		CheckVectorsClose(yaw * PgE::Vector3::Right, PgE::Vector3::Forward);
	}
}

TEST_CASE("A transform applies scale, then rotation, then translation")
{
	const PgE::Transform transform{.Position = {.X = 10.0f, .Y = 0.0f, .Z = 0.0f},
								   .Rotation = PgE::Quaternion::FromAxisAngle(PgE::Vector3::Up, QuarterTurn),
								   .Scale = {.X = 2.0f, .Y = 1.0f, .Z = 1.0f}};

	// Scale stretches along the object's own X before the yaw swings it onto world +Y. Scaling after the
	// rotation would stretch world X instead and land the point somewhere else entirely.
	CheckVectorsClose(transform.TransformPoint(PgE::Vector3::Right), PgE::Vector3{.X = 10.0f, .Y = 2.0f, .Z = 0.0f});

	// A direction ignores both translation and scale.
	CheckVectorsClose(transform.TransformDirection(PgE::Vector3::Right), PgE::Vector3::Forward);
}

TEST_CASE("Inverse transform operations undo their forward counterparts")
{
	const PgE::Transform transform{.Position = {.X = 3.0f, .Y = -4.0f, .Z = 12.0f},
								   .Rotation = PgE::Quaternion::FromAxisAngle(PgE::Vector3{.X = 1.0f, .Y = 2.0f, .Z = 3.0f}, 0.7f),
								   .Scale = {.X = 2.0f, .Y = 0.5f, .Z = 3.0f}};

	constexpr PgE::Vector3 point{.X = 1.0f, .Y = 2.0f, .Z = -0.5f};
	CheckVectorsClose(transform.InverseTransformPoint(transform.TransformPoint(point)), point);
	CheckVectorsClose(transform.InverseTransformDirection(transform.TransformDirection(point)), point);
}

TEST_CASE("The basis accessors are the rotated world axes")
{
	const PgE::Transform transform{.Rotation = PgE::Quaternion::FromAxisAngle(PgE::Vector3::Up, QuarterTurn)};

	CheckVectorsClose(transform.GetRight(), PgE::Vector3::Forward);
	CheckVectorsClose(transform.GetForward(), -PgE::Vector3::Right);
	CheckVectorsClose(transform.GetUp(), PgE::Vector3::Up);
}

TEST_CASE("ToMatrix agrees with TransformPoint and holds translation in the last column")
{
	const PgE::Transform transform{.Position = {.X = 5.0f, .Y = 6.0f, .Z = 7.0f},
								   .Rotation = PgE::Quaternion::FromAxisAngle(PgE::Vector3::Up, QuarterTurn),
								   .Scale = {.X = 2.0f, .Y = 3.0f, .Z = 4.0f}};

	const PgE::Matrix4x4 matrix = transform.ToMatrix();

	CHECK(matrix.Columns[3].X == doctest::Approx(5.0f));
	CHECK(matrix.Columns[3].Y == doctest::Approx(6.0f));
	CHECK(matrix.Columns[3].Z == doctest::Approx(7.0f));
	CHECK(matrix.Columns[3].W == doctest::Approx(1.0f));

	constexpr PgE::Vector3 point{.X = 1.0f, .Y = -2.0f, .Z = 0.5f};
	const PgE::Vector4 transformed = matrix * PgE::Vector4{.X = point.X, .Y = point.Y, .Z = point.Z, .W = 1.0f};
	const PgE::Vector3 expected = transform.TransformPoint(point);

	CheckVectorsClose(PgE::Vector3{.X = transformed.X, .Y = transformed.Y, .Z = transformed.Z}, expected);
	CHECK(transformed.W == doctest::Approx(1.0f));
}

TEST_CASE("Matrix multiplication applies the right operand first")
{
	constexpr PgE::Transform translation{.Position = {.X = 1.0f, .Y = 0.0f, .Z = 0.0f}};
	const PgE::Transform rotation{.Rotation = PgE::Quaternion::FromAxisAngle(PgE::Vector3::Up, QuarterTurn)};

	// Rotating a translated point differs from translating a rotated one, which is what makes the order
	// observable rather than a convention with no consequence.
	constexpr PgE::Vector4 origin{.X = 0.0f, .Y = 0.0f, .Z = 0.0f, .W = 1.0f};
	const PgE::Vector4 rotateThenTranslate = translation.ToMatrix() * rotation.ToMatrix() * origin;
	const PgE::Vector4 translateThenRotate = rotation.ToMatrix() * translation.ToMatrix() * origin;

	CheckVectorsClose(PgE::Vector3{.X = rotateThenTranslate.X, .Y = rotateThenTranslate.Y, .Z = rotateThenTranslate.Z},
					  PgE::Vector3{.X = 1.0f, .Y = 0.0f, .Z = 0.0f});
	CheckVectorsClose(PgE::Vector3{.X = translateThenRotate.X, .Y = translateThenRotate.Y, .Z = translateThenRotate.Z},
					  PgE::Vector3{.X = 0.0f, .Y = 1.0f, .Z = 0.0f});
}

TEST_CASE("Normalizing a zero vector yields zero rather than NaNs")
{
	CHECK(PgE::Vector3::Normalize(PgE::Vector3::Zero) == PgE::Vector3::Zero);
	CHECK(PgE::Vector3{.X = 3.0f, .Y = 4.0f, .Z = 0.0f}.Length() == doctest::Approx(5.0f));
}

TEST_CASE("The math types reflect as named aggregates")
{
	// The reason the engine owns these types rather than aliasing a library's: a class template
	// specialization has no identifier, so a field of one reflects nameless and gives serialization and
	// the C# boundary nothing to key on.

	const PgE::TypeInfo& transformType = PgE::TypeMetaOf<PgE::Transform>();
	CHECK(transformType.GetIdentifier() == "Transform");
	REQUIRE(transformType.GetFields().size() == 3);

	const PgE::FieldInfo& position = transformType.GetFields()[0];
	CHECK(position.GetIdentifier() == "Position");
	CHECK(position.GetTypeInfo().GetIdentifier() == "Vector3");
	CHECK(position.GetTypeInfo().GetFields().size() == 3);
	CHECK(position.GetTypeInfo().GetFields()[0].GetIdentifier() == "X");
}

TEST_CASE("Euler angles round-trip through the quaternion they name")
{
	const PgE::EulerAngles authored{.Pitch = PgE::ToRadians(20.0f), .Yaw = PgE::ToRadians(-115.0f), .Roll = PgE::ToRadians(45.0f)};

	const PgE::EulerAngles recovered = PgE::Quaternion::FromEulerAngles(authored).ToEulerAngles();

	CHECK(recovered.Pitch == doctest::Approx(authored.Pitch).epsilon(Tolerance));
	CHECK(recovered.Yaw == doctest::Approx(authored.Yaw).epsilon(Tolerance));
	CHECK(recovered.Roll == doctest::Approx(authored.Roll).epsilon(Tolerance));
}

TEST_CASE("Each Euler angle rotates about the axis its name claims")
{
	const float quarterTurn = PgE::ToRadians(90.0f);

	SUBCASE("yaw alone turns forward toward left")
	{
		const PgE::Quaternion rotation = PgE::Quaternion::FromEulerAngles({.Yaw = quarterTurn});
		CheckVectorsClose(rotation * PgE::Vector3::Right, PgE::Vector3::Forward);
	}

	SUBCASE("pitch alone lifts forward toward up")
	{
		const PgE::Quaternion rotation = PgE::Quaternion::FromEulerAngles({.Pitch = quarterTurn});
		CheckVectorsClose(rotation * PgE::Vector3::Forward, PgE::Vector3::Up);
	}

	SUBCASE("roll alone tips up toward right")
	{
		const PgE::Quaternion rotation = PgE::Quaternion::FromEulerAngles({.Roll = quarterTurn});
		CheckVectorsClose(rotation * PgE::Vector3::Up, PgE::Vector3::Right);
	}
}

TEST_CASE("Yaw is applied about the world up axis, not the pitched one")
{
	// The composition order is the part a different convention would change: yawing after a pitch must
	// still swing the object around world Z, which is what a character controller expects.

	const PgE::Quaternion rotation = PgE::Quaternion::FromEulerAngles({.Pitch = PgE::ToRadians(90.0f), .Yaw = PgE::ToRadians(90.0f)});

	// Pitch takes forward to up; yaw about world Z then leaves it there, since up lies on the yaw axis.
	CheckVectorsClose(rotation * PgE::Vector3::Forward, PgE::Vector3::Up);

	// Right, however, is carried around by the yaw.
	CheckVectorsClose(rotation * PgE::Vector3::Right, PgE::Vector3::Forward);
}

TEST_CASE("At gimbal lock the whole turn is reported as yaw")
{
	// Straight up leaves yaw and roll sharing one axis, so no triple is more correct than another. The
	// choice is pinned here because authoring UI and serialized data both read it.

	const PgE::Quaternion straightUp =
		PgE::Quaternion::FromEulerAngles({.Pitch = PgE::ToRadians(90.0f), .Yaw = PgE::ToRadians(30.0f), .Roll = PgE::ToRadians(50.0f)});

	const PgE::EulerAngles recovered = straightUp.ToEulerAngles();

	CHECK(recovered.Pitch == doctest::Approx(PgE::ToRadians(90.0f)).epsilon(Tolerance));
	CHECK(recovered.Roll == doctest::Approx(0.0f).epsilon(Tolerance));

	// Yaw absorbs both turns, so the rotation it names is still the one authored.
	CheckVectorsClose(PgE::Quaternion::FromEulerAngles(recovered) * PgE::Vector3::Right, straightUp * PgE::Vector3::Right);
}

TEST_CASE("Degree and radian conversion round-trips")
{
	CHECK(PgE::ToDegrees(PgE::ToRadians(137.5f)) == doctest::Approx(137.5f));
	CHECK(PgE::ToRadians(180.0f) == doctest::Approx(std::numbers::pi_v<float>));
}

TEST_CASE("A rotation about a degenerate axis is rejected")
{
	// Normalizing a zero-length axis yields Zero, which would make the result a non-unit quaternion and,
	// at half a turn, exactly the zero quaternion. Rejecting is better than handing back something that
	// scales everything it is later composed with.
	CHECK_THROWS_AS(PgE::Quaternion::FromAxisAngle(PgE::Vector3::Zero, 1.0f), PgETest::ContractViolationError);
	CHECK_THROWS_AS(PgE::Quaternion::FromAxisAngle(PgE::Vector3::Cross(PgE::Vector3::Up, PgE::Vector3::Up), 1.0f), PgETest::ContractViolationError);
}

TEST_CASE("Inverting a transform through a collapsed axis is rejected")
{
	// A zero scale component is a normal authoring action (flattening an object) and makes the transform
	// non-invertible, so the inverse is refused rather than returning infinities.
	const PgE::Transform flattened{.Scale = {.X = 1.0f, .Y = 1.0f, .Z = 0.0f}};

	CHECK_THROWS_AS(flattened.InverseTransformPoint(PgE::Vector3::One), PgETest::ContractViolationError);

	// The direction form divides by nothing, so it stays available on the same transform.
	CheckVectorsClose(flattened.InverseTransformDirection(PgE::Vector3::Forward), PgE::Vector3::Forward);
}
