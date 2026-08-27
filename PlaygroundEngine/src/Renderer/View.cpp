module PlaygroundEngine.Renderer.View;

import std;

namespace PgE
{
	Matrix4x4 MakeWorldToViewMatrix(const Vector3 position, const Quaternion rotation)
	{
		// The rotation is orthonormal, so the transposed basis below is its inverse.

		const Vector3 right = rotation * Vector3::Right;
		const Vector3 up = rotation * Vector3::Up;
		const Vector3 backward = -(rotation * Vector3::Forward);

		return Matrix4x4{
			.Columns = {
				{.X = right.X, .Y = up.X, .Z = backward.X, .W = 0.0f},
				{.X = right.Y, .Y = up.Y, .Z = backward.Y, .W = 0.0f},
				{.X = right.Z, .Y = up.Z, .Z = backward.Z, .W = 0.0f},
				{.X = -Vector3::Dot(right, position), .Y = -Vector3::Dot(up, position), .Z = -Vector3::Dot(backward, position), .W = 1.0f},
			}};
	}

	Matrix4x4 MakePerspectiveProjectionMatrix(const float verticalFieldOfViewRadians,
											  const float aspectRatio,
											  const float nearPlaneDistance,
											  const float farPlaneDistance)
	{
		const float focalLength = 1.0f / std::tan(verticalFieldOfViewRadians * 0.5f);
		const float depthRange = nearPlaneDistance - farPlaneDistance;

		// The negated Y term is Vulkan's downward clip axis; the Z terms put near at depth 0 and far at 1.

		return Matrix4x4{.Columns = {
							 {.X = focalLength / aspectRatio, .Y = 0.0f, .Z = 0.0f, .W = 0.0f},
							 {.X = 0.0f, .Y = -focalLength, .Z = 0.0f, .W = 0.0f},
							 {.X = 0.0f, .Y = 0.0f, .Z = farPlaneDistance / depthRange, .W = -1.0f},
							 {.X = 0.0f, .Y = 0.0f, .Z = (nearPlaneDistance * farPlaneDistance) / depthRange, .W = 0.0f},
						 }};
	}
}
