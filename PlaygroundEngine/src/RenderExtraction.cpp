module PlaygroundEngine.RenderExtraction;

import PlaygroundEngine.Ecs.CameraComponent;
import PlaygroundEngine.Ecs.MeshComponent;
import PlaygroundEngine.Math;
import PlaygroundEngine.Renderer.Mesh;
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

		void ExtractMeshes(const Ecs& world, const MeshCatalog& meshes, ExtractedFrame& frame)
		{
			frame.Meshes.clear();

			for (const auto& [entity, mesh] : world.GetComponentsWithEntities<MeshComponent>())
			{
				// A path the catalog never resolved draws nothing. The load error was already reported
				// where the catalog was filled, so failing again here would report it once a frame.

				const MeshHandle handle = meshes.Find(mesh->MeshPath);
				if (handle.Index == MeshHandle::InvalidIndex)
				{
					continue;
				}

				const std::shared_ptr<TransformComponent> transform = world.TryGetComponent<TransformComponent>(entity);
				if (transform == nullptr)
				{
					continue;
				}

				// The component's Transform base is the whole payload, so the slice is the conversion, not a
				// loss. Spelled out because an implicit one reads as an accident.

				frame.Meshes.push_back(ExtractedMesh{.Mesh = handle, .Placement = static_cast<const Transform&>(*transform)});
			}
		}
	}

	void ExtractFrame(const Ecs& world, const MeshCatalog& meshes, ExtractedFrame& frame)
	{
		frame.View = ExtractView(world);
		ExtractMeshes(world, meshes, frame);
	}
}
