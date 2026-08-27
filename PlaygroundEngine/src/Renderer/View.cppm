export module PlaygroundEngine.Renderer.View;

import PlaygroundEngine.Math;

import std;

namespace PgE
{
	export struct ExtractedView
	{
		Vector3 Position = Vector3::Zero;
		Quaternion Rotation = Quaternion::Identity;

		float VerticalFieldOfViewRadians = ToRadians(45.0f);
		float NearPlaneDistance = 0.1f;
		float FarPlaneDistance = 1000.0f;
	};

	// View space is +X right, +Y up, -Z forward. See docs/CoreConventions.md (View & Clip Space).
	export Matrix4x4 MakeWorldToViewMatrix(Vector3 position, Quaternion rotation);

	// Produces Vulkan clip space: Y down, depth 0..1. See docs/CoreConventions.md (View & Clip Space).
	export Matrix4x4 MakePerspectiveProjectionMatrix(float verticalFieldOfViewRadians,
													 float aspectRatio,
													 float nearPlaneDistance,
													 float farPlaneDistance)
		pre(verticalFieldOfViewRadians > 0.0f && verticalFieldOfViewRadians < std::numbers::pi_v<float>) pre(aspectRatio > 0.0f)
			pre(nearPlaneDistance > 0.0f) pre(farPlaneDistance > nearPlaneDistance);
}
