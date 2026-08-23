export module PlaygroundGame.Ecs.EntityNameComponent;

import PlaygroundEngine.Ecs.Component;
import PlaygroundEngine.DebugUi;
import std;

namespace PgG
{
	export class EntityNameComponent : public PgE::Component
	{
	public:
		[[= PgE::DrawDebug{}]] std::string Name;
	};
}
