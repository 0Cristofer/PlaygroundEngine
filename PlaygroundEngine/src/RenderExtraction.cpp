module PlaygroundEngine.RenderExtraction;

import PlaygroundEngine.Ecs.CameraComponent;
import PlaygroundEngine.Math;
import PlaygroundEngine.Renderer.View;

import std;

namespace PgE
{
	namespace
	{
		// A component is open data and the debug panel drags these through zero, where the projection
		// would divide by them.

		constexpr float MinimumFieldOfViewDegrees = 1.0f;
		constexpr float MaximumFieldOfViewDegrees = 179.0f;
		constexpr float MinimumNearPlaneDistance = 0.001f;
		constexpr float MinimumPlaneSeparation = 0.001f;

		ExtractedView ToExtractedView(const Transform& placement, const CameraComponent& camera)
		{
			const float nearPlaneDistance = std::max(camera.NearPlaneDistance, MinimumNearPlaneDistance);

			return ExtractedView{.Position = placement.Position,
								 .Rotation = placement.Rotation,
								 .VerticalFieldOfViewRadians =
									 ToRadians(std::clamp(camera.VerticalFieldOfViewDegrees, MinimumFieldOfViewDegrees, MaximumFieldOfViewDegrees)),
								 .NearPlaneDistance = nearPlaneDistance,
								 .FarPlaneDistance = std::max(camera.FarPlaneDistance, nearPlaneDistance + MinimumPlaneSeparation)};
		}

		ExtractedView ExtractView(const Ecs& world)
		{
			// Entities come back in id order, so the lowest id wins without sorting.

			for (const auto& [entity, camera] : world.GetComponentsWithEntities<CameraComponent>())
			{
				if (const std::shared_ptr<TransformComponent> transform = world.TryGetComponent<TransformComponent>(entity); transform != nullptr)
				{
					return ToExtractedView(*transform, *camera);
				}
			}

			return ExtractedView{};
		}
	}

	void ExtractFrame(const Ecs& world, ExtractedFrame& frame)
	{
		frame.View = ExtractView(world);
	}
}
