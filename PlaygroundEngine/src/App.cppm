export module PlaygroundEngine.App;

import PlaygroundEngine.World;

namespace PgE
{
	export class EngineContext
	{
		// The app's handle to the engine after boot: register phase callbacks, subscribe to published
		// signals/queues, reach curated per-system extension APIs. Vends capabilities, never lifecycle
		// (Boot/Run/Shutdown stay on Engine). Empty until the first system exists.
	};

	export class AppBase
	{
	public:
		virtual ~AppBase() = default;

		virtual void OnBooted(EngineContext& engine) = 0;
		virtual void OnStartRun(World* world) = 0;
	};
}
