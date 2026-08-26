export module PlaygroundEngine.Math:Transform;

import :Types;

import PlaygroundEngine.DebugUi.Annotations;

namespace PgE
{
	export struct Transform
	{
		// World space, always: there is no hierarchy, so no local variant exists. Scale is applied in the
		// object's own frame, then rotation, then translation (M = T * R * S).

		[[= DrawDebug{}]] Vector3 Position = Vector3::Zero;
		[[= DrawDebug{}]] Quaternion Rotation = Quaternion::Identity;
		[[= DrawDebug{}]] Vector3 Scale = Vector3::One;

		Vector3 TransformPoint(Vector3 point) const;
		Vector3 TransformDirection(Vector3 direction) const;

		// A zero scale component collapses an axis, so the transform is not invertible and the division
		// below would hand back infinities. Callers that let a user author scale (a debug panel, an
		// importer) clamp it away from zero rather than relying on a result here.
		Vector3 InverseTransformPoint(Vector3 point) const pre(Scale.X != 0.0f) pre(Scale.Y != 0.0f) pre(Scale.Z != 0.0f);

		Vector3 InverseTransformDirection(Vector3 direction) const;

		Vector3 GetRight() const;
		Vector3 GetForward() const;
		Vector3 GetUp() const;

		Matrix4x4 ToMatrix() const;

		// Deliberately absent: composition (operator*) and an Inverse() returning a Transform. Both are
		// lossy under non-uniform scale, where the result contains shear that no position/rotation/scale
		// triple can hold. Callers needing either compose matrices, which makes the lossy step visible.

		// A member earns its place here only by being a pure function of the three fields that means the
		// same thing to every consumer. See docs/CoreConventions.md (Transform) for what stays outside.
	};

	inline Vector3 Transform::TransformPoint(const Vector3 point) const
	{
		return Position + Rotation * (Scale * point);
	}

	inline Vector3 Transform::TransformDirection(const Vector3 direction) const
	{
		return Rotation * direction;
	}

	Vector3 Transform::InverseTransformPoint(const Vector3 point) const
	{
		return Quaternion::Conjugate(Rotation) * (point - Position) / Scale;
	}

	inline Vector3 Transform::InverseTransformDirection(const Vector3 direction) const
	{
		return Quaternion::Conjugate(Rotation) * direction;
	}

	inline Vector3 Transform::GetRight() const
	{
		return Rotation * Vector3::Right;
	}

	inline Vector3 Transform::GetForward() const
	{
		return Rotation * Vector3::Forward;
	}

	inline Vector3 Transform::GetUp() const
	{
		return Rotation * Vector3::Up;
	}

	inline Matrix4x4 Transform::ToMatrix() const
	{
		// The rotation's basis vectors are its columns, so scaling each column by the matching component
		// applies scale before rotation (M = T * R * S) without building three matrices to multiply.

		const Vector3 rotatedRight = GetRight() * Scale.X;
		const Vector3 rotatedForward = GetForward() * Scale.Y;
		const Vector3 rotatedUp = GetUp() * Scale.Z;

		return Matrix4x4{.Columns = {
							 {.X = rotatedRight.X, .Y = rotatedRight.Y, .Z = rotatedRight.Z, .W = 0.0f},
							 {.X = rotatedForward.X, .Y = rotatedForward.Y, .Z = rotatedForward.Z, .W = 0.0f},
							 {.X = rotatedUp.X, .Y = rotatedUp.Y, .Z = rotatedUp.Z, .W = 0.0f},
							 {.X = Position.X, .Y = Position.Y, .Z = Position.Z, .W = 1.0f},
						 }};
	}
}
