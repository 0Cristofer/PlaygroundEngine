export module PlaygroundGame.Ecs.PlayerControlledComponent;

import PlaygroundEngine.Ecs;
import PlaygroundEngine.DebugUi;

namespace PgG
{
	export class PlayerControlledComponent : public PgE::Component
	{
	public:
		[[= PgE::DrawDebug{}]] float MoveSpeedMetersPerSecond = 3.0f;
	};
}
