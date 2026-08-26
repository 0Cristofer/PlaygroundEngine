export module PlaygroundEngine.Math:Types;

import std;

// Definitions live here, not in an implementation unit, and the non-constexpr ones are marked inline:
// GCC hands an importer a body across a module boundary only for an inline or constexpr function, and a
// contract on such a function fails to link. See CLAUDE.md (Toolchain constraints).

namespace PgE
{
	export struct Vector3
	{
		float X = 0.0f;
		float Y = 0.0f;
		float Z = 0.0f;

		static const Vector3 Zero;
		static const Vector3 One;

		// The world basis from docs/CoreConventions.md: right-handed, Z up. Named here so no system
		// hardcodes an axis triple.
		static const Vector3 Right;
		static const Vector3 Forward;
		static const Vector3 Up;

		float Length() const;

		static constexpr float Dot(const Vector3 left, const Vector3 right)
		{
			return left.X * right.X + left.Y * right.Y + left.Z * right.Z;
		}

		static constexpr Vector3 Cross(const Vector3 left, const Vector3 right)
		{
			return Vector3{
				.X = left.Y * right.Z - left.Z * right.Y, .Y = left.Z * right.X - left.X * right.Z, .Z = left.X * right.Y - left.Y * right.X};
		}

		constexpr float LengthSquared() const
		{
			return Dot(*this, *this);
		}

		// Returns Zero for a zero-length input rather than NaNs, so a caller summing input axes can
		// normalize unconditionally.
		static Vector3 Normalize(Vector3 value);

		static float Distance(Vector3 from, Vector3 to);
	};

	export struct Vector4
	{
		float X = 0.0f;
		float Y = 0.0f;
		float Z = 0.0f;
		float W = 0.0f;

		static const Vector4 Zero;
		static const Vector4 One;
	};

	// A rotation in the form humans author and read. Not a runtime representation: the composition order
	// on FromEulerAngles is one of several conventions, and the quaternion it converts to is the truth.
	export struct EulerAngles
	{
		float Pitch = 0.0f;
		float Yaw = 0.0f;
		float Roll = 0.0f;
	};

	// Stored x, y, z, w and always spelled in that order.
	export struct Quaternion
	{
		float X = 0.0f;
		float Y = 0.0f;
		float Z = 0.0f;
		float W = 1.0f;

		static const Quaternion Identity;

		static constexpr Quaternion Conjugate(const Quaternion value)
		{
			return Quaternion{.X = -value.X, .Y = -value.Y, .Z = -value.Z, .W = value.W};
		}

		static Quaternion Normalize(Quaternion value);

		// Right-hand rule: a positive angle is counter-clockwise looking down the axis toward the origin. A
		// zero-length axis names no rotation and would yield a non-unit quaternion, so it is rejected; a
		// caller deriving an axis from a cross product guards against parallel inputs first.
		static Quaternion FromAxisAngle(Vector3 axis, float angleRadians) pre(axis.LengthSquared() > 0.0f);

		// Yaw about world Z, then pitch about the yawed right axis, then roll about the resulting forward
		// axis, which is what a character or camera wants. Spelled as Yaw * Pitch * Roll, since the right
		// operand applies first.
		static Quaternion FromEulerAngles(EulerAngles angles);

		// Not the inverse of FromEulerAngles for every input: several triples name the same rotation, and at
		// a pitch of +/-90 degrees yaw and roll collapse into one axis, where this reports the whole turn as
		// yaw and zero roll. Authoring UI keeps its own triple rather than reconverting.
		EulerAngles ToEulerAngles() const;
	};

	// Column-major, matching the column-vector convention (v' = M * v) and what the shader uploads expect.
	// Columns[3] is the translation.
	export struct Matrix4x4
	{
		Vector4 Columns[4];

		static const Matrix4x4 Identity;
	};

	inline constexpr Vector3 Vector3::Zero{.X = 0.0f, .Y = 0.0f, .Z = 0.0f};
	inline constexpr Vector3 Vector3::One{.X = 1.0f, .Y = 1.0f, .Z = 1.0f};
	inline constexpr Vector3 Vector3::Right{.X = 1.0f, .Y = 0.0f, .Z = 0.0f};
	inline constexpr Vector3 Vector3::Forward{.X = 0.0f, .Y = 1.0f, .Z = 0.0f};
	inline constexpr Vector3 Vector3::Up{.X = 0.0f, .Y = 0.0f, .Z = 1.0f};

	inline constexpr Vector4 Vector4::Zero{.X = 0.0f, .Y = 0.0f, .Z = 0.0f, .W = 0.0f};
	inline constexpr Vector4 Vector4::One{.X = 1.0f, .Y = 1.0f, .Z = 1.0f, .W = 1.0f};

	inline constexpr Quaternion Quaternion::Identity{.X = 0.0f, .Y = 0.0f, .Z = 0.0f, .W = 1.0f};

	inline constexpr Matrix4x4 Matrix4x4::Identity{.Columns = {
													   {.X = 1.0f, .Y = 0.0f, .Z = 0.0f, .W = 0.0f},
													   {.X = 0.0f, .Y = 1.0f, .Z = 0.0f, .W = 0.0f},
													   {.X = 0.0f, .Y = 0.0f, .Z = 1.0f, .W = 0.0f},
													   {.X = 0.0f, .Y = 0.0f, .Z = 0.0f, .W = 1.0f},
												   }};

	export constexpr bool operator==(const Vector3 left, const Vector3 right)
	{
		return left.X == right.X && left.Y == right.Y && left.Z == right.Z;
	}

	export constexpr Vector3 operator-(const Vector3 value)
	{
		return Vector3{.X = -value.X, .Y = -value.Y, .Z = -value.Z};
	}

	export constexpr Vector3 operator+(const Vector3 left, const Vector3 right)
	{
		return Vector3{.X = left.X + right.X, .Y = left.Y + right.Y, .Z = left.Z + right.Z};
	}

	export constexpr Vector3 operator-(const Vector3 left, const Vector3 right)
	{
		return Vector3{.X = left.X - right.X, .Y = left.Y - right.Y, .Z = left.Z - right.Z};
	}

	// Componentwise, which is what a scale vector means. The dot and cross products are named functions.
	export constexpr Vector3 operator*(const Vector3 left, const Vector3 right)
	{
		return Vector3{.X = left.X * right.X, .Y = left.Y * right.Y, .Z = left.Z * right.Z};
	}

	export constexpr Vector3 operator/(const Vector3 left, const Vector3 right)
	{
		return Vector3{.X = left.X / right.X, .Y = left.Y / right.Y, .Z = left.Z / right.Z};
	}

	export constexpr Vector3 operator*(const Vector3 vector, const float scalar)
	{
		return Vector3{.X = vector.X * scalar, .Y = vector.Y * scalar, .Z = vector.Z * scalar};
	}

	export constexpr Vector3 operator*(const float scalar, const Vector3 vector)
	{
		return vector * scalar;
	}

	export constexpr Vector3 operator/(const Vector3 vector, const float scalar)
	{
		return Vector3{.X = vector.X / scalar, .Y = vector.Y / scalar, .Z = vector.Z / scalar};
	}

	export constexpr Vector3& operator+=(Vector3& target, const Vector3 value)
	{
		target = target + value;
		return target;
	}

	export constexpr Vector3& operator-=(Vector3& target, const Vector3 value)
	{
		target = target - value;
		return target;
	}

	export constexpr Vector3& operator*=(Vector3& target, const float scalar)
	{
		target = target * scalar;
		return target;
	}

	export constexpr bool operator==(const Vector4 left, const Vector4 right)
	{
		return left.X == right.X && left.Y == right.Y && left.Z == right.Z && left.W == right.W;
	}

	export constexpr Vector4 operator-(const Vector4 value)
	{
		return Vector4{.X = -value.X, .Y = -value.Y, .Z = -value.Z, .W = -value.W};
	}

	export constexpr Vector4 operator+(const Vector4 left, const Vector4 right)
	{
		return Vector4{.X = left.X + right.X, .Y = left.Y + right.Y, .Z = left.Z + right.Z, .W = left.W + right.W};
	}

	export constexpr Vector4 operator-(const Vector4 left, const Vector4 right)
	{
		return Vector4{.X = left.X - right.X, .Y = left.Y - right.Y, .Z = left.Z - right.Z, .W = left.W - right.W};
	}

	export constexpr Vector4 operator*(const Vector4 vector, const float scalar)
	{
		return Vector4{.X = vector.X * scalar, .Y = vector.Y * scalar, .Z = vector.Z * scalar, .W = vector.W * scalar};
	}

	export constexpr Vector4 operator*(const float scalar, const Vector4 vector)
	{
		return vector * scalar;
	}

	export constexpr Vector4 operator/(const Vector4 vector, const float scalar)
	{
		return Vector4{.X = vector.X / scalar, .Y = vector.Y / scalar, .Z = vector.Z / scalar, .W = vector.W / scalar};
	}

	export constexpr bool operator==(const Quaternion left, const Quaternion right)
	{
		return left.X == right.X && left.Y == right.Y && left.Z == right.Z && left.W == right.W;
	}

	// Applies right first, then left, matching the column-vector convention used for matrices.
	export constexpr Quaternion operator*(const Quaternion left, const Quaternion right)
	{
		return Quaternion{.X = left.W * right.X + left.X * right.W + left.Y * right.Z - left.Z * right.Y,
						  .Y = left.W * right.Y - left.X * right.Z + left.Y * right.W + left.Z * right.X,
						  .Z = left.W * right.Z + left.X * right.Y - left.Y * right.X + left.Z * right.W,
						  .W = left.W * right.W - left.X * right.X - left.Y * right.Y - left.Z * right.Z};
	}

	// v + 2 * cross(q.xyz, cross(q.xyz, v) + q.w * v), the standard sandwich product with the two
	// conjugate multiplications folded out.
	export constexpr Vector3 operator*(const Quaternion rotation, const Vector3 vector)
	{
		const Vector3 axis{.X = rotation.X, .Y = rotation.Y, .Z = rotation.Z};
		const Vector3 intermediate = Vector3::Cross(axis, vector) + vector * rotation.W;

		return vector + Vector3::Cross(axis, intermediate) * 2.0f;
	}

	export constexpr Vector4 operator*(const Matrix4x4& matrix, const Vector4 vector)
	{
		const Vector4& columnX = matrix.Columns[0];
		const Vector4& columnY = matrix.Columns[1];
		const Vector4& columnZ = matrix.Columns[2];
		const Vector4& columnW = matrix.Columns[3];

		return Vector4{.X = columnX.X * vector.X + columnY.X * vector.Y + columnZ.X * vector.Z + columnW.X * vector.W,
					   .Y = columnX.Y * vector.X + columnY.Y * vector.Y + columnZ.Y * vector.Z + columnW.Y * vector.W,
					   .Z = columnX.Z * vector.X + columnY.Z * vector.Y + columnZ.Z * vector.Z + columnW.Z * vector.W,
					   .W = columnX.W * vector.X + columnY.W * vector.Y + columnZ.W * vector.Z + columnW.W * vector.W};
	}

	// Column vectors, so the right operand is applied first.
	export constexpr Matrix4x4 operator*(const Matrix4x4& left, const Matrix4x4& right)
	{
		return Matrix4x4{.Columns = {
							 left * right.Columns[0],
							 left * right.Columns[1],
							 left * right.Columns[2],
							 left * right.Columns[3],
						 }};
	}

	inline float Vector3::Length() const
	{
		return std::sqrt(LengthSquared());
	}

	inline Vector3 Vector3::Normalize(const Vector3 value)
	{
		const float length = value.Length();
		if (length == 0.0f)
		{
			return Vector3::Zero;
		}

		return value / length;
	}

	inline float Vector3::Distance(const Vector3 from, const Vector3 to)
	{
		return (to - from).Length();
	}

	inline Quaternion Quaternion::Normalize(const Quaternion value)
	{
		const float length = std::sqrt(value.X * value.X + value.Y * value.Y + value.Z * value.Z + value.W * value.W);
		if (length == 0.0f)
		{
			return Quaternion::Identity;
		}

		return Quaternion{.X = value.X / length, .Y = value.Y / length, .Z = value.Z / length, .W = value.W / length};
	}

	Quaternion Quaternion::FromAxisAngle(const Vector3 axis, const float angleRadians)
	{
		const Vector3 unitAxis = Vector3::Normalize(axis);
		const float halfAngle = angleRadians * 0.5f;
		const float sine = std::sin(halfAngle);

		return Quaternion{.X = unitAxis.X * sine, .Y = unitAxis.Y * sine, .Z = unitAxis.Z * sine, .W = std::cos(halfAngle)};
	}

	inline Quaternion Quaternion::FromEulerAngles(const EulerAngles angles)
	{
		return FromAxisAngle(Vector3::Up, angles.Yaw) * FromAxisAngle(Vector3::Right, angles.Pitch) * FromAxisAngle(Vector3::Forward, angles.Roll);
	}

	inline EulerAngles Quaternion::ToEulerAngles() const
	{
		// The rotated basis vectors are the rotation matrix's columns, so the entries the extraction needs
		// are read off them rather than by building a matrix: forward.Z is sin(pitch), and the remaining two
		// angles come from the pairs that carry cos(pitch) as a common factor.

		const Vector3 right = *this * Vector3::Right;
		const Vector3 forward = *this * Vector3::Forward;
		const Vector3 up = *this * Vector3::Up;

		const float sinPitch = std::clamp(forward.Z, -1.0f, 1.0f);

		// Within a tenth of a degree of vertical, yaw and roll have collapsed onto one axis and their terms
		// are noise, so the whole turn is reported as yaw. Pitch is set to exactly vertical rather than
		// taken from asin, whose slope is unbounded at the pole. See docs/CoreConventions.md (Transform).
		if (constexpr float gimbalLockThreshold = 0.999999f; std::abs(sinPitch) > gimbalLockThreshold)
		{
			return EulerAngles{.Pitch = std::copysign(std::numbers::pi_v<float> / 2.0f, sinPitch), .Yaw = std::atan2(right.Y, right.X), .Roll = 0.0f};
		}

		return EulerAngles{.Pitch = std::asin(sinPitch), .Yaw = std::atan2(-forward.X, forward.Y), .Roll = std::atan2(-right.Z, up.Z)};
	}

	export inline float ToDegrees(const float radians)
	{
		return radians * (180.0f / std::numbers::pi_v<float>);
	}

	export inline float ToRadians(const float degrees)
	{
		return degrees * (std::numbers::pi_v<float> / 180.0f);
	}
}
