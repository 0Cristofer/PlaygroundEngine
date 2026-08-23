export module PlaygroundEngine.Ecs.PositionComponent;

import PlaygroundEngine.Ecs.Component;
import PlaygroundEngine.DebugUi;

namespace PgE
{
	export class PositionComponent : public Component
	{
	public:
		[[= DrawDebug{}]] int Position = 5;
	};
}
