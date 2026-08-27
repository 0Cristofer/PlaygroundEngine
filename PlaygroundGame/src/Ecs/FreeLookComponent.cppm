export module PlaygroundGame.Ecs.FreeLookComponent;

import PlaygroundEngine.Ecs;
import PlaygroundEngine.DebugUi;

namespace PgG
{
	export class FreeLookComponent : public PgE::Component
	{
	public:
		[[= PgE::DrawDebug{}]] float TurnSpeedRadiansPerSecond = 1.5f;
		[[= PgE::DrawDebug{}]] float MoveSpeedMetersPerSecond = 1.5f;
	};
}
