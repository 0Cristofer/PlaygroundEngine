export module PlaygroundEngine.Ecs.CameraComponent;

import PlaygroundEngine.Ecs;
import PlaygroundEngine.DebugUi.Annotations;

namespace PgE
{
	export class CameraComponent : public Component
	{
	public:
		[[= DrawDebug{}]] float VerticalFieldOfViewDegrees = 45.0f;
		[[= DrawDebug{}]] float NearPlaneDistance = 0.1f;
		[[= DrawDebug{}]] float FarPlaneDistance = 1000.0f;
	};
}
