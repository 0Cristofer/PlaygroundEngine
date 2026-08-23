export module PlaygroundEngine.Ecs.Entity;

import PlaygroundEngine.DebugUi;

namespace PgE
{
	export struct Entity
	{
		// A value handle, never an owner: copied freely, compared by id, and meaningful only to the Ecs
		// that issued it. Id 0 is the invalid handle, so a default-constructed Entity names nothing.
		static constexpr int InvalidId = 0;

		[[= DrawDebug{}]] int Id = InvalidId;

		bool operator==(const Entity& other) const = default;
	};
}
