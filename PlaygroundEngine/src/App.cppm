export module PlaygroundEngine.App;

import PlaygroundEngine.World;

namespace PgE
{
	export class AppBase
	{
	public:
		virtual ~AppBase() = default;

		virtual void OnInitialized() = 0;
		virtual void OnRun(World* world) = 0;
	};
}
